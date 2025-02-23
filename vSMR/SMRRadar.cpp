#include "stdafx.h"
#include "Resource.h"
#include "SMRRadar.hpp"
#include <cmath>
#include <boost/geometry.hpp>
#include "PlaneShapeBuilder.h"
using namespace std::string_literals;

ULONG_PTR m_gdiplusToken;
CPoint mouseLocation(0, 0);
string TagBeingDragged;
int LeaderLineDefaultlenght = 50;

//
// Cursor Things
//

bool initCursor = true;
HCURSOR smrCursor = nullptr;
bool standardCursor; // switches between mouse cursor and pointer cursor when moving tags
bool customCursor; // use SMR version or default windows mouse symbol
WNDPROC gSourceProc;
HWND pluginWindow;
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

map<string, string> CSMRRadar::vStripsStands;

map<int, CInsetWindow*> appWindows;

inline double closest(std::vector<double> const& vec, double value)
{
	auto const it = std::lower_bound(vec.begin(), vec.end(), value);
	if (it == vec.end()) { return -1; }

	return *it;
};

inline bool IsTagBeingDragged(const string& c)
{
	return TagBeingDragged == c;
}

bool mouseWithin(const CRect& rect)
{
	return mouseLocation.x >= rect.left + 1
		&& mouseLocation.x <= rect.right - 1
		&& mouseLocation.y >= rect.top + 1
		&& mouseLocation.y <= rect.bottom - 1;
}


void CSMRRadar::draw_target(TagDrawingContext& tdc, CRadarTarget& rt, const bool alt_mode)
{
	constexpr int mem_buffer_size = 200;
	constexpr unsigned int border_width = 2;
	constexpr int border_growth = border_width + 1;

	const std::string callsign = rt.GetCallsign();
	if (!rt.IsValid())
		return;

	const bool is_asel = strcmp(rt.GetCallsign(), GetPlugIn()->FlightPlanSelectASEL().GetCallsign()) == 0;
	const int dimming = is_asel ? 0 : TAG_DIMMING;

	CRadarTargetPositionData RtPos = rt.GetPosition();
	POINT acPosPix = ConvertCoordFromPositionToPixel(RtPos.GetPosition());
	CFlightPlan fp = GetPlugIn()->FlightPlanSelect(rt.GetCallsign());
	int reportedGs = RtPos.GetReportedGS();

	// Filtering the targets

	bool isAcDisplayed = isVisible(rt);

	// Alt mode makes me pretend its correlated
	bool AcisCorrelated = alt_mode || IsCorrelated(fp, rt);

	if (!filters.show_free && strlen(rt.GetCorrelatedFlightPlan().GetTrackingControllerId()) == 0)
		isAcDisplayed = false;

	if (!filters.show_nonmine && strlen(fp.GetTrackingControllerId()) != 0 && !fp.GetTrackingControllerIsMe() && !fp.
		GetCoordinatedNextController())
		isAcDisplayed = false;

	if (!AcisCorrelated && belux_promode && !belux_promode_easy)
		isAcDisplayed = false;

	if (std::find(ReleasedTracks.begin(), ReleasedTracks.end(), rt.GetSystemID()) != ReleasedTracks.end())
		isAcDisplayed = false;

	if (!filters.show_on_blocks && gate_target->isOnBlocks(this, &rt))
		isAcDisplayed = false;

	// NSTS shows up as "" here
	if (!filters.show_nsts && strlen(fp.GetGroundState()) == 0)
		isAcDisplayed = false;
	if (!filters.show_stup && strcmp(fp.GetGroundState(), "STUP") == 0)
		isAcDisplayed = false;

	if (!isAcDisplayed && !alt_mode)
	{
		return;
	}

	if (TagAngles.find(callsign) == TagAngles.end())
	{
		TagAngles[callsign] = 360 - 45.0f;
	}

	// Would the start of a right-aligned tag be to the left of the tag start?
	const bool right_align = fmod(abs(TagAngles[callsign] + 90), 360) > 180;

	// Set up an offscreen buffer to draw to
	// that way, we can measure the tag while drawing and position it _perfectly_.
	Bitmap mem_buffer(mem_buffer_size, mem_buffer_size);
	Graphics graphics(&mem_buffer);

	// We can't just share the start, otherwise we draw out of buffer
	const POINT tag_start = right_align
		                        ? POINT{mem_buffer_size - border_growth, border_growth}
		                        : POINT{border_growth, border_growth};

	TagTypes TagType = TagTypes::Departure;
	TagTypes ColorTagType = TagTypes::Departure;

	if (fp.IsValid() && strcmp(fp.GetFlightPlanData().GetDestination(), getActiveAirport().c_str()) == 0)
	{
		// Circuit aircraft are treated as departures; not arrivals
		if (strcmp(fp.GetFlightPlanData().GetOrigin(), getActiveAirport().c_str()) != 0)
		{
			TagType = TagTypes::Arrival;
			ColorTagType = TagTypes::Arrival;
		}
	}

	if (reportedGs > 50)
	{
		TagType = TagTypes::Airborne;

		// Is "use_departure_arrival_coloring" enabled? if not, then use the airborne colors
		bool useDepArrColors = CurrentConfig->getActiveProfile()["labels"]["airborne"]["use_departure_arrival_coloring"]
			.GetBool();
		if (!useDepArrColors)
		{
			ColorTagType = TagTypes::Airborne;
		}
	}

	if (!AcisCorrelated &&
		(belux_promode && belux_promode_easy))
	{
		TagType = TagTypes::Uncorrelated;
		ColorTagType = TagTypes::Uncorrelated;
	}

	map<string, string> TagReplacingMap = GenerateTagData(rt, fp, IsCorrelated(fp, rt),
	                                                      CurrentConfig->getActiveProfile()["filters"]["pro_mode"][
		                                                      "enable"].GetBool() || belux_promode,
	                                                      GetPlugIn()->GetTransitionAltitude(),
	                                                      CurrentConfig->getActiveProfile()["labels"][
		                                                      "use_aspeed_for_gate"].GetBool(), getActiveAirport());

	// ----- Generating the clickable map -----
	map<string, int> TagClickableMap;
	TagClickableMap[TagReplacingMap["callsign"]] = TAG_CITEM_CALLSIGN;
	TagClickableMap[TagReplacingMap["actype"]] = TAG_CITEM_FPBOX;
	TagClickableMap[TagReplacingMap["sctype"]] = TAG_CITEM_FPBOX;
	TagClickableMap[TagReplacingMap["sqerror"]] = TAG_CITEM_FPBOX;
	TagClickableMap[TagReplacingMap["deprwy"]] = TAG_CITEM_RWY;
	TagClickableMap[TagReplacingMap["seprwy"]] = TAG_CITEM_RWY;
	TagClickableMap[TagReplacingMap["arvrwy"]] = TAG_CITEM_RWY;
	TagClickableMap[TagReplacingMap["srvrwy"]] = TAG_CITEM_RWY;
	TagClickableMap[TagReplacingMap["gate"]] = TAG_CITEM_GATE;
	TagClickableMap[TagReplacingMap["sate"]] = TAG_CITEM_GATE;
	TagClickableMap[TagReplacingMap["flightlevel"]] = TAG_CITEM_NO;
	TagClickableMap[TagReplacingMap["gs"]] = TAG_CITEM_NO;
	TagClickableMap[TagReplacingMap["tendency"]] = TAG_CITEM_NO;
	TagClickableMap[TagReplacingMap["wake"]] = TAG_CITEM_FPBOX;
	TagClickableMap[TagReplacingMap["tssr"]] = TAG_CITEM_NO;
	TagClickableMap[TagReplacingMap["asid"]] = TagClickableMap[TagReplacingMap["ssid"]] = TAG_CITEM_SID;
	TagClickableMap[TagReplacingMap["origin"]] = TAG_CITEM_FPBOX;
	TagClickableMap[TagReplacingMap["dest"]] = TAG_CITEM_FPBOX;
	TagClickableMap[TagReplacingMap["systemid"]] = TAG_CITEM_NO;
	TagClickableMap[TagReplacingMap["groundstatus"]] = TAG_CITEM_GROUNDSTATUS;
	TagClickableMap[TagReplacingMap["uk_stand"]] = TAG_CITEM_UKSTAND;
	TagClickableMap[TagReplacingMap["scratch_pad"]] = TAG_CITEM_GATE;
	TagClickableMap["SSR/FPL"] = TAG_CITEM_CALLSIGN; // Not really, but close enough ya know

	//
	// ----- Now the hard part, drawing (using gdi+) -------
	//

	// First we need to figure out the tag size

	int TagWidth = 0, TagHeight = 0;


	//const Value& LabelLines = (*tdc.labels_settings)[UIHelper::getEnumString(TagType).c_str()]["definition"];
	const optional<vector<vector<string>>> parsed_label_lines = UIHelper::parse_label_lines(
		(*tdc.labels_settings)[UIHelper::getEnumString(TagType).c_str()]["definition"]);
	vector<vector<string>> LabelLines = parsed_label_lines.value_or(vector<vector<string>>());

	Color definedBackgroundColor = CurrentConfig->getConfigColor(
		(*tdc.labels_settings)[UIHelper::getEnumString(ColorTagType).c_str()]["background_color"]);

	if (ColorTagType == TagTypes::Departure)
	{
		if (!TagReplacingMap["asid"].empty() && CurrentConfig->isSidColorAvail(
			TagReplacingMap["asid"], getActiveAirport()))
		{
			definedBackgroundColor = CurrentConfig->getSidColor(TagReplacingMap["asid"], getActiveAirport());
		}
		if (fp.GetFlightPlanData().GetPlanType()[0] == 'I' && TagReplacingMap["asid"].empty() && (*tdc.labels_settings)[
			UIHelper::getEnumString(ColorTagType).c_str()].HasMember("nosid_color"))
		{
			definedBackgroundColor = CurrentConfig->getConfigColor(
				(*tdc.labels_settings)[UIHelper::getEnumString(ColorTagType).c_str()]["nosid_color"]);
		}
	}
	if (TagReplacingMap["actype"] == "NoFPL" && (*tdc.labels_settings)[UIHelper::getEnumString(ColorTagType).c_str()].
		HasMember("nofpl_color"))
	{
		definedBackgroundColor = CurrentConfig->getConfigColor(
			(*tdc.labels_settings)[UIHelper::getEnumString(ColorTagType).c_str()]["nofpl_color"]);
	}


	const Color TagBackgroundColor = ColorManager->get_corrected_color("label", dimming, definedBackgroundColor);


	SolidBrush FontColor(ColorManager
		->get_corrected_color("label",
		                      CurrentConfig->getConfigColor(
			                      (*tdc.labels_settings)[UIHelper::getEnumString(
				                      ColorTagType).c_str()]["text_color"])));

	const bool is_assr_err = !TagReplacingMap["sqerror"].empty() && TagReplacingMap["actype"] != "NoFPL";
	const bool is_rimcas_err = RimcasInstance->getAlert(callsign) != CRimcas::NoAlert;

	if (is_assr_err && show_err_lines)
	{
		auto val = vector<string>();
		val.emplace_back("SSR/FPL");
		LabelLines.insert(LabelLines.begin(), std::move(val));
	}

	if (is_rimcas_err)
	{
		auto val = vector<string>();
		val.emplace_back("ALERT");
		LabelLines.insert(LabelLines.begin(), std::move(val));
	}

	vector<std::tuple<int, CRect>> interactables;


	vector<PointF> border_points;
	border_points.reserve(2 + 2 * LabelLines.size());

	border_points.push_back(PointF{
		static_cast<Gdiplus::REAL>(tag_start.x),
		static_cast<Gdiplus::REAL>(tag_start.y)
	});
	for (unsigned int i = 0; i < LabelLines.size(); i++)
	{
		const auto line = LabelLines[i];
		vector<string> lineStringArray;

		int TempTagWidth = 0;
		int TempTagHeight = 0;

		/*
		 * Okay, breathe, I got you.
		 * If we're left aligning, iterate forwards.
		 * if right aligning, we iterate in the inverse order. Okay? Lovely
		 */
		for (size_t el = right_align ? line.size() : 0; right_align ? el-- > 0 : el < line.size();
		     right_align ? 0 : ++el)
		{
			RectF mesureRect = RectF(0, 0, 0, 0);
			const string element_was = line[el];
			string element = line[el];

			for (auto& kv : TagReplacingMap)
			{
				replaceAll(element, kv.first, kv.second);
			}

			lineStringArray.push_back(element);

			if (element == "" && line.size() == 1)
			{
				continue;
			}

			wstring wstr = wstring(element.begin(), element.end());
			Gdiplus::Font* font = customFonts[currentFontSize];
			if ((element_was == "callsign" || element_was == "SSR/FPL" || i == 0 || element_was == "ALERT") && TagType != TagTypes::Uncorrelated)
			{
				font = customFonts[currentFontSize + 10];
			}

			graphics.MeasureString(wstr.c_str(), wcslen(wstr.c_str()),
			                       font, PointF(0, 0), &Gdiplus::StringFormat(), &mesureRect);

			// Setup text colors
			const Brush* color = nullptr;

			if (is_asel && ColorTagType == TagTypes::Airborne)
			{
				color = tdc.asel_text_color->Clone();
			}

			if (TagReplacingMap["sqerror"].size() > 0 && strcmp(element.c_str(), TagReplacingMap["sqerror"].c_str()) ==
				0)
				color = tdc.squawk_error_color->Clone();

			if (RimcasInstance->getAlert(rt.GetCallsign()) != CRimcas::NoAlert)
				color = &FontColor;

			// Ground tag colors
			if (strcmp(element.c_str(), "PUSH") == 0)
				color = tdc.ground_push_color;
			if (strcmp(element.c_str(), "TAXI") == 0)
				color = tdc.ground_taxi_color;
			if (strcmp(element.c_str(), "DEPA") == 0)
				color = tdc.ground_depa_color;

			if (color == nullptr) // default case
			{
				color = &FontColor;
			}

			// Drawing!
			const auto draw_start = right_align
				                        ? tag_start.x - TempTagWidth - floor(mesureRect.Width) - tdc.blank_width
				                        : tag_start.x + TempTagWidth;

			// Adjust background to RIMCAS color, if this row is ALERT
			const Color BackgroundColor = element == "ALERT"
				                              ? RimcasInstance->GetAircraftColor(
					                              rt.GetCallsign(), TagBackgroundColor, TagBackgroundColor,
					                              CurrentConfig->getConfigColor(
						                              CurrentConfig->getActiveProfile()["rimcas"][
							                              "background_color_stage_one"]
					                              ),
					                              CurrentConfig->getConfigColor(
						                              CurrentConfig->getActiveProfile()["rimcas"][
							                              "background_color_stage_two"]
					                              ))
				                              : TagBackgroundColor;
			const SolidBrush backgroundBrush(BackgroundColor);
			graphics.FillRectangle(&backgroundBrush, static_cast<long>(draw_start), tag_start.y + TagHeight,
			                       static_cast<int>(mesureRect.Width) + tdc.blank_width,
			                       static_cast<int>(mesureRect.Height));

			const RectF layoutRect(draw_start,
			                       static_cast<Gdiplus::REAL>(tag_start.y + TagHeight),
			                       mesureRect.Width + tdc.blank_width,
			                       mesureRect.Height);
			graphics.DrawString(wstr.c_str(), wcslen(wstr.c_str()), font, layoutRect, &Gdiplus::StringFormat(),
			                    color);

			CRect ItemRect(floor(layoutRect.GetLeft()), floor(layoutRect.GetTop()),
			               floor(layoutRect.GetRight()),
			               floor(layoutRect.GetBottom()));
			interactables.push_back({TagClickableMap[element], ItemRect});

			TempTagWidth += static_cast<int>(mesureRect.GetRight());
			TempTagHeight = max(TempTagHeight, static_cast<int>(mesureRect.GetBottom()));

			// If we're not looking at the last element
			if ((right_align && el != 0) || (!right_align && el != line.size() - 1))
			{
				TempTagWidth += tdc.blank_width;
			}
			else if (!right_align)
			{
				// Left aligned and last element
				PointF line_top_right(floor(layoutRect.GetRight()),
				                      floor(layoutRect.GetTop()));
				PointF line_bottom_right(floor(layoutRect.GetRight()),
				                         floor(layoutRect.GetBottom()));
				border_points.push_back(line_top_right);
				border_points.push_back(line_bottom_right);
			}
			if (right_align && el == 0)
			{
				PointF line_top_left(floor(layoutRect.GetLeft()),
				                     floor(layoutRect.GetTop()));
				PointF line_bottom_left(floor(layoutRect.GetLeft()),
				                        floor(layoutRect.GetBottom()));
				border_points.push_back(line_top_left);
				border_points.push_back(line_bottom_left);
			}
		}


		TagWidth = max(TagWidth, TempTagWidth);
		TagHeight += TempTagHeight;
	}

	border_points.push_back(PointF{border_points.front().X, border_points.back().Y});

	CRect TagBackgroundRect(tag_start.x, tag_start.y, tag_start.x + TagWidth,
	                        tag_start.y + TagHeight);


	// Drawing the border
	const bool is_rimcas_stage_two = RimcasInstance->getAlert(callsign) == CRimcas::StageTwo;
	if ((is_asel || is_assr_err || is_rimcas_stage_two) && ColorTagType != TagTypes::Airborne)
	{
		Color border_color = is_assr_err
			                     ? is_asel
				                       ? Color::Orange
				                       : Color::Red
			                     : is_asel
			                     ? Color::Yellow
			                     : Color::AlphaMask;

		if (is_rimcas_stage_two)
		{
			border_color = Color(255, 0, 255);
		}

		// Width of border. 4 is realistic-ish. I've taken that into account above. Sorry for magic numbers
		// We should expand the polygon
		const auto grown_points = UIHelper::grow_border(border_points, border_growth, right_align);

		Gdiplus::Pen pen(ColorManager->get_corrected_color("label", border_color), border_width);
		pen.SetAlignment(Gdiplus::PenAlignmentInset);
		graphics.DrawPolygon(&pen, grown_points.data(), grown_points.size());
	}

	const auto angle_rad = DegToRad(TagAngles[callsign]);

	// This needs to be ever so slightly further from the TagCenter,
	// as to make the length var the distance between tag edge and PRS.
	// Luckily, we know the angle between center and PRS and the tag dimensions,
	// so some sin(alpha) = ((TagHeight/2)/llen) should help
	const auto extension = min(
		abs((TagHeight / 2) / sin(angle_rad)),
		abs((TagWidth / 2) / cos(angle_rad))
	);

	POINT TagCenter;
	int length = LeaderLineDefaultlenght;
	if (TagLeaderLineLength.find(callsign) != TagLeaderLineLength.end())
		length = TagLeaderLineLength[callsign];

	length += extension;

	TagCenter.x = long(acPosPix.x + float(length * cos(DegToRad(TagAngles[callsign]))));
	TagCenter.y = long(acPosPix.y + float(length * sin(DegToRad(TagAngles[callsign]))));

	const POINT tag_top_left = POINT{TagCenter.x - (TagWidth / 2), TagCenter.y - (TagHeight / 2)};
	const INT x1 = right_align ? mem_buffer_size - TagWidth - 3 * border_growth : 0;

	// Drawing the symbol to tag line to actual screen, then blit the actual tag
	const PointF acPosF = PointF(static_cast<Gdiplus::REAL>(acPosPix.x), static_cast<Gdiplus::REAL>(acPosPix.y));
	const Pen leaderLinePen = Pen(ColorManager->get_corrected_color("label", dimming, Color::White));
	vector<PointF> transformed_border_points(border_points.size());
	for (auto border_point : border_points)
	{
		border_point.X += tag_top_left.x - x1;
		border_point.Y += tag_top_left.y;
		transformed_border_points.push_back(border_point);
	}

	Logger::info("Leader line..");
	UIHelper::drawLeaderLine(transformed_border_points, acPosF, &leaderLinePen, tdc.graphics);


	// Blit to screen without copying
	tdc.graphics->DrawImage(&mem_buffer,
	                        static_cast<INT>(tag_top_left.x), static_cast<INT>(tag_top_left.y), x1, 0,
	                        TagWidth + 3 * border_growth, TagHeight + 2 * border_growth,
	                        Unit::UnitPixel
	);

	// Adding the tag screen object
	TagBackgroundRect.MoveToXY(tag_top_left.x + border_growth, tag_top_left.y + border_growth);
	TagBackgroundRect.right += border_growth;
	tagAreas[rt.GetCallsign()] = TagBackgroundRect;

	const auto bottom_line = GetBottomLine(rt.GetCallsign());
	for (auto value : interactables)
	{
		CRect loc = std::get<CRect>(value);
		loc.MoveToXY(loc.left + tag_top_left.x - x1, loc.top + tag_top_left.y);
		AddScreenObject(std::get<int>(value), rt.GetCallsign(), loc, true,
		                bottom_line.c_str());
	}


	/*
	 * Let me explain...
	 * The tag item itself is only really used to ASEL the aircraft, or drag it.
	 * Why would you click on a part that doesn't really appear to be part of the non-rectangular tag??
	 * I'll leave this line here for posterity
	 */
	//AddScreenObject(DRAWING_TAG, rt.GetCallsign(), TagBackgroundRect, true, GetBottomLine(rt.GetCallsign()).c_str());
}

void CSMRRadar::draw_context_menu(HDC hdc)
{
	if (!this->context_menu_for.has_value())
		return;

	const ContextMenuData flight = this->context_menu_for.value();

	if (flight.callsign != GetPlugIn()->RadarTargetSelectASEL().GetCallsign())
	{
		this->context_menu_for = std::nullopt;
		return;
	}

	constexpr int line_height = 15;
	constexpr int width = 100;
	constexpr int buffer = 20;
	constexpr int lines = 4;
	constexpr int total_height = lines * line_height;
	constexpr float text_size = 8.0;
	constexpr float callsign_size = 10.0;

	const Color background_color = this->ColorManager->get_corrected_color("label", Color(0, 0, 0));
	const Color contrast_color = this->ColorManager->get_corrected_color("label", Color(255, 255, 255));

	const WCHAR* font_name = L"Euroscope";
	const Gdiplus::Font font(font_name, text_size);
	const Gdiplus::Font callsign_font(font_name, callsign_size);

	const wstring wcallsign = wstring(flight.callsign.begin(), flight.callsign.end());
	StringFormat callsign_format;
	callsign_format.SetAlignment(StringAlignmentCenter);
	callsign_format.SetLineAlignment(StringAlignmentCenter);
	StringFormat generic_format;
	generic_format.SetLineAlignment(StringAlignmentCenter);

	Graphics graphics(hdc);
	const RECT radar_area = this->GetRadarArea();
	// If closer to the right, draw to the left with buffer, else to the right with buffer
	const int offset_x = abs(context_menu_pos.x - radar_area.left) > abs(context_menu_pos.x - radar_area.right)
		                     ? context_menu_pos.x - width - buffer
		                     : context_menu_pos.x + buffer;
	const POINT draw_at = POINT{offset_x, context_menu_pos.y};

	const SolidBrush background_brush(background_color);
	const SolidBrush contrast_brush(contrast_color);
	const Pen contrast_pen(contrast_color);

	const Rect background(draw_at.x, draw_at.y, width, total_height);
	graphics.FillRectangle(&background_brush, background);
	graphics.DrawRectangle(&contrast_pen, background);

	// Callsign line
	graphics.FillRectangle(&contrast_brush, draw_at.x, draw_at.y, width, line_height);
	graphics.DrawString(wcallsign.c_str(), -1, &callsign_font, RectF(draw_at.x, draw_at.y, width, line_height),
	                    &callsign_format, &background_brush);

	// Release
	const WCHAR* release_label = this->is_manually_released(flight.system_id.c_str()) ? L"XDrop" : L"Drop";
	graphics.DrawString(release_label, -1, &font, RectF(draw_at.x, draw_at.y + 1 * line_height, width, line_height),
	                    &generic_format, &contrast_brush);
	AddScreenObject(CONTEXT_RELEASE, flight.system_id.c_str(), RECT{
		                draw_at.x, draw_at.y + 1 * line_height, draw_at.x + width, draw_at.y + 2 * line_height
	                }, false,
	                GetBottomLine(flight.callsign.c_str()).c_str());;

	// Acquire
	const WCHAR* acquire_label = this->is_manually_correlated(flight.system_id.c_str()) ? L"XAcquire" : L"Acquire";
	graphics.DrawString(acquire_label, -1, &font, RectF(draw_at.x, draw_at.y + 2 * line_height, width, line_height),
	                    &generic_format, &contrast_brush);
	AddScreenObject(CONTEXT_ACQUIRE, flight.system_id.c_str(), RECT{
		                draw_at.x, draw_at.y + 2 * line_height, draw_at.x + width, draw_at.y + 3 * line_height
	                }, false,
	                GetBottomLine(flight.callsign.c_str()).c_str());;

	const std::string strip_seven = GetPlugIn()->RadarTargetSelectASEL().GetCorrelatedFlightPlan().
	                                             GetControllerAssignedData().GetFlightStripAnnotation(7);
	const bool cleared = strip_seven.find("K") != std::string::npos;
	const WCHAR* land_label = cleared ? L"XLand" : L"Land";
	graphics.DrawString(land_label, -1, &font, RectF(draw_at.x, draw_at.y + 3 * line_height, width, line_height),
	                    &generic_format, &contrast_brush);
	AddScreenObject(CONTEXT_LAND, flight.system_id.c_str(), RECT{
		                draw_at.x, draw_at.y + 3 * line_height, draw_at.x + width, draw_at.y + 4 * line_height
	                }, false,
	                GetBottomLine(flight.callsign.c_str()).c_str());;

	graphics.ReleaseHDC(hdc);
}

CSMRRadar::CSMRRadar()
{
	Logger::info("CSMRRadar::CSMRRadar()");

	// Initializing randomizer
	srand(static_cast<unsigned>(time(nullptr)));

	// Initialize GDI+
	GdiplusStartupInput gdiplusStartupInput;
	GdiplusStartup(&m_gdiplusToken, &gdiplusStartupInput, nullptr);

	// Getting the DLL file folder
	GetModuleFileNameA(HINSTANCE(&__ImageBase), DllPathFile, sizeof(DllPathFile));
	DllPath = DllPathFile;
	DllPath.resize(DllPath.size() - strlen("vSMR.dll"));

	ConfigPath = DllPath + "\\vSMR_Profiles.json";

	Logger::info("Loading callsigns");

	// Creating the RIMCAS instance
	if (Callsigns == nullptr)
		Callsigns = new CCallsignLookup();

	// We can look in three places for this file:
	// 1. Within the plugin directory
	// 2. In the ICAO folder of a GNG package
	// 3. In the working directory of EuroScope
	fs::path possible_paths[3];
	possible_paths[0] = fs::path(DllPath) / fs::path("ICAO_Airlines.txt");
	possible_paths[1] = fs::path(DllPath).parent_path().parent_path() / fs::path("ICAO") /
		fs::path("ICAO_Airlines.txt");
	possible_paths[2] = fs::path(DllPath).parent_path().parent_path().parent_path() / fs::path("ICAO") / fs::path(
		"ICAO_Airlines.txt");

	for (const auto& p : possible_paths)
	{
		Logger::info("Trying to read callsigns from: " + p.string());
		if (fs::exists(p))
		{
			Logger::info("Found callsign file!");
			Callsigns->readFile(p.string());

			break;
		}
	};

	Logger::info("Loading RIMCAS & Config");
	// Creating the RIMCAS instance
	if (RimcasInstance == nullptr)
		RimcasInstance = new CRimcas();

	// Loading up the config file
	if (CurrentConfig == nullptr)
		CurrentConfig = new CConfig(ConfigPath);

	if (ColorManager == nullptr)
		ColorManager = new CColorManager();

	standardCursor = true;
	ActiveAirport = "EGKK";

	// Setting up the data for the 2 approach windows
	appWindowDisplays[1] = false;
	appWindowDisplays[2] = false;
	appWindows[1] = new CInsetWindow(APPWINDOW_ONE);
	appWindows[2] = new CInsetWindow(APPWINDOW_TWO);

	Logger::info("Loading profile");

	this->CSMRRadar::LoadProfile("Default");

	this->CSMRRadar::LoadCustomFont();

	this->CSMRRadar::RefreshAirportActivity();

	gate_target = new GateTarget();
	gate_target->loadGates();
}

CSMRRadar::~CSMRRadar()
{
	Logger::info(string(__FUNCSIG__));
	try
	{
		//this->OnAsrContentToBeSaved();
		//this->EuroScopePlugInExitCustom();
	}
	catch (exception& e)
	{
		stringstream s;
		s << e.what() << endl;
		AfxMessageBox(string("Error occurred " + s.str()).c_str());
	}
	// Shutting down GDI+
	GdiplusShutdown(m_gdiplusToken);
	delete CurrentConfig;
}

void CSMRRadar::CorrelateCursor()
{
	if (NeedCorrelateCursor)
	{
		if (standardCursor)
		{
			smrCursor = CopyCursor(
				(HCURSOR)::LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDC_SMRCORRELATE), IMAGE_CURSOR, 0, 0,
					LR_SHARED));

			AFX_MANAGE_STATE(AfxGetStaticModuleState());
			ASSERT(smrCursor);
			SetCursor(smrCursor);
			standardCursor = false;
		}
	}
	else
	{
		if (!standardCursor)
		{
			if (customCursor)
			{
				smrCursor = CopyCursor(
					(HCURSOR)::LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDC_SMRCURSOR), IMAGE_CURSOR, 0, 0,
						LR_SHARED));
			}
			else
			{
				smrCursor = (HCURSOR)::LoadCursor(nullptr, IDC_ARROW);
			}

			AFX_MANAGE_STATE(AfxGetStaticModuleState());
			ASSERT(smrCursor);
			SetCursor(smrCursor);
			standardCursor = true;
		}
	}
}

void CSMRRadar::LoadCustomFont()
{
	Logger::info(string(__FUNCSIG__));
	// Loading the custom font if there is one in use
	customFonts.clear();

	const Value& FSizes = CurrentConfig->getActiveProfile()["font"]["sizes"];
	string font_name = CurrentConfig->getActiveProfile()["font"]["font_name"].GetString();
	const wstring buffer = wstring(font_name.begin(), font_name.end());
	const int titleOffset = CurrentConfig->getActiveProfile()["font"]["title_offset"].GetInt();


	customFonts[1] = new Gdiplus::Font(buffer.c_str(), Gdiplus::REAL(FSizes["one"].GetInt()), Gdiplus::FontStyleRegular,
	                                   Gdiplus::UnitPixel);
	customFonts[2] = new Gdiplus::Font(buffer.c_str(), Gdiplus::REAL(FSizes["two"].GetInt()), Gdiplus::FontStyleRegular,
	                                   Gdiplus::UnitPixel);
	customFonts[3] = new Gdiplus::Font(buffer.c_str(), Gdiplus::REAL(FSizes["three"].GetInt()),
	                                   Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
	customFonts[4] = new Gdiplus::Font(buffer.c_str(), Gdiplus::REAL(FSizes["four"].GetInt()),
	                                   Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);
	customFonts[5] = new Gdiplus::Font(buffer.c_str(), Gdiplus::REAL(FSizes["five"].GetInt()),
	                                   Gdiplus::FontStyleRegular, Gdiplus::UnitPixel);

	customFonts[11] = new Gdiplus::Font(buffer.c_str(), Gdiplus::REAL(FSizes["one"].GetInt() + titleOffset),
	                                    Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
	customFonts[12] = new Gdiplus::Font(buffer.c_str(), Gdiplus::REAL(FSizes["two"].GetInt() + titleOffset),
	                                    Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
	customFonts[13] = new Gdiplus::Font(buffer.c_str(), Gdiplus::REAL(FSizes["three"].GetInt() + titleOffset),
	                                    Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
	customFonts[14] = new Gdiplus::Font(buffer.c_str(), Gdiplus::REAL(FSizes["four"].GetInt() + titleOffset),
	                                    Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
	customFonts[15] = new Gdiplus::Font(buffer.c_str(), Gdiplus::REAL(FSizes["five"].GetInt() + titleOffset),
	                                    Gdiplus::FontStyleBold, Gdiplus::UnitPixel);
}

void CSMRRadar::LoadProfile(string profileName)
{
	Logger::info(string(__FUNCSIG__));
	// Loading the new profile
	CurrentConfig->setActiveProfile(profileName);

	// Loading all the new data
	const Value& RimcasTimer = CurrentConfig->getActiveProfile()["rimcas"]["timer"];
	const Value& RimcasTimerLVP = CurrentConfig->getActiveProfile()["rimcas"]["timer_lvp"];

	vector<int> RimcasNorm;
	for (SizeType i = 0; i < RimcasTimer.Size(); i++)
	{
		RimcasNorm.push_back(RimcasTimer[i].GetInt());
	}

	vector<int> RimcasLVP;
	for (SizeType i = 0; i < RimcasTimerLVP.Size(); i++)
	{
		RimcasLVP.push_back(RimcasTimerLVP[i].GetInt());
	}
	RimcasInstance->setCountdownDefinition(RimcasNorm, RimcasLVP);
	LeaderLineDefaultlenght = CurrentConfig->getActiveProfile()["labels"]["leader_line_length"].GetInt();

	customCursor = CurrentConfig->isCustomCursorUsed();

	// Reloading the fonts
	this->LoadCustomFont();
}

void CSMRRadar::OnAsrContentLoaded(bool Loaded)
{
	Logger::info(string(__FUNCSIG__));
	const char* p_value;

	// ReSharper disable CppZeroConstantCanBeReplacedWithNullptr
	if ((p_value = GetDataFromAsr("Airport")) != NULL)
		setActiveAirport(p_value);

	if ((p_value = GetDataFromAsr("ActiveProfile")) != NULL)
		this->LoadProfile(string(p_value));

	if ((p_value = GetDataFromAsr("FontSize")) != NULL)
		currentFontSize = atoi(p_value);

	if ((p_value = GetDataFromAsr("Afterglow")) != NULL)
		Afterglow = atoi(p_value) == 1 ? true : false;

	if ((p_value = GetDataFromAsr("Symbol")) != NULL)
		ColorManager->update_brightness("symbol", atoi(p_value));

	if ((p_value = GetDataFromAsr("AppTrailsDots")) != NULL)
		Trail_App = atoi(p_value);

	if ((p_value = GetDataFromAsr("GndTrailsDots")) != NULL)
		Trail_Gnd = atoi(p_value);

	if ((p_value = GetDataFromAsr("PredictedLine")) != NULL)
		PredictedLength = atoi(p_value);

	if ((p_value = GetDataFromAsr("BeluxProMode")) != NULL)
		belux_promode = (strcmp(p_value, "on") == 0);

	if ((p_value = GetDataFromAsr("BeluxProModeEasy")) != NULL)
		belux_promode_easy = (strcmp(p_value, "on") == 0);

	if ((p_value = GetDataFromAsr("ShiftTopBar")) != NULL)
		shift_top_bar = (strcmp(p_value, "on") == 0);

	if ((p_value = GetDataFromAsr("ShowErrLines")) != NULL)
		show_err_lines = (strcmp(p_value, "on") == 0);

	if ((p_value = GetDataFromAsr("Filters")) != NULL)
	{
		filters = filters_from_char(*p_value);
	}

	if ((p_value = GetDataFromAsr("AltModeKeyCode")) != NULL)
	{
		alt_mode_keycode = atoi(p_value);
	}

	string temp;

	for (int i = 1; i < 3; i++)
	{
		string prefix = "SRW" + std::to_string(i);

		if ((p_value = GetDataFromAsr(string(prefix + "TopLeftX").c_str())) != NULL)
			appWindows[i]->m_Area.left = atoi(p_value);

		if ((p_value = GetDataFromAsr(string(prefix + "TopLeftY").c_str())) != NULL)
			appWindows[i]->m_Area.top = atoi(p_value);

		if ((p_value = GetDataFromAsr(string(prefix + "BottomRightX").c_str())) != NULL)
			appWindows[i]->m_Area.right = atoi(p_value);

		if ((p_value = GetDataFromAsr(string(prefix + "BottomRightY").c_str())) != NULL)
			appWindows[i]->m_Area.bottom = atoi(p_value);

		if ((p_value = GetDataFromAsr(string(prefix + "OffsetX").c_str())) != NULL)
			appWindows[i]->m_Offset.x = atoi(p_value);

		if ((p_value = GetDataFromAsr(string(prefix + "OffsetY").c_str())) != NULL)
			appWindows[i]->m_Offset.y = atoi(p_value);


		if ((p_value = GetDataFromAsr(string(prefix + "Filter").c_str())) != NULL)
			appWindows[i]->m_Filter = atoi(p_value);

		if ((p_value = GetDataFromAsr(string(prefix + "Scale").c_str())) != NULL)
			appWindows[i]->m_Scale = atoi(p_value);

		if ((p_value = GetDataFromAsr(string(prefix + "Rotation").c_str())) != NULL)
			appWindows[i]->m_Rotation = atoi(p_value);

		if ((p_value = GetDataFromAsr(string(prefix + "Display").c_str())) != NULL)
			appWindowDisplays[i] = atoi(p_value) == 1 ? true : false;
	}

	// Auto load the airport config on ASR opened.
	for (CSectorElement rwy = GetPlugIn()->SectorFileElementSelectFirst(SECTOR_ELEMENT_RUNWAY);
	     rwy.IsValid();
	     rwy = GetPlugIn()->SectorFileElementSelectNext(rwy, SECTOR_ELEMENT_RUNWAY))
	{
		if (!startsWith(getActiveAirport().c_str(), rwy.GetAirportName())) continue;

		string name = rwy.GetRunwayName(0) + string(" / ") + rwy.GetRunwayName(1);

		if (rwy.IsElementActive(true, 0) || rwy.IsElementActive(true, 1) || rwy.IsElementActive(false, 0) || rwy.
			IsElementActive(false, 1))
		{
			RimcasInstance->toggleMonitoredRunwayDep(name);
			if (rwy.IsElementActive(false, 0) || rwy.IsElementActive(false, 1))
			{
				RimcasInstance->toggleMonitoredRunwayArr(name);
			}
		}
	}

	// ReSharper restore CppZeroConstantCanBeReplacedWithNullptr
}

void CSMRRadar::OnAsrContentToBeSaved()
{
	Logger::info(string(__FUNCSIG__));


	SaveDataToAsr("Airport", "Active airport for RIMCAS", getActiveAirport().c_str());

	SaveDataToAsr("ActiveProfile", "vSMR active profile", CurrentConfig->getActiveProfileName().c_str());

	SaveDataToAsr("FontSize", "vSMR font size", std::to_string(currentFontSize).c_str());

	SaveDataToAsr("Afterglow", "vSMR Afterglow enabled", std::to_string(int(Afterglow)).c_str());

	SaveDataToAsr("Symbol", "vSMR Symbol brightness", std::to_string(ColorManager->get_brightness("symbol")).c_str());

	SaveDataToAsr("AppTrailsDots", "vSMR APPR Trail Dots", std::to_string(Trail_App).c_str());

	SaveDataToAsr("GndTrailsDots", "vSMR GRND Trail Dots", std::to_string(Trail_Gnd).c_str());

	SaveDataToAsr("PredictedLine", "vSMR Predicted Track Lines", std::to_string(PredictedLength).c_str());

	SaveDataToAsr("BeluxProMode", "vSMR Belux pro mode", (belux_promode ? "on" : "off"));
	SaveDataToAsr("BeluxProModeEasy", "vSMR Belux pro mode - easy version", (belux_promode_easy ? "on" : "off"));

	SaveDataToAsr("ShiftTopBar", "Shift top menu bar downwards", shift_top_bar ? "on" : "off");

	SaveDataToAsr("ShowErrLines", "Show TAG error lines", show_err_lines ? "on" : "off");

	SaveDataToAsr("AltModeKeyCode", "Keycode to trigger alt mode", std::to_string(alt_mode_keycode).c_str());

	const char filter_char = char_from_filters(filters);
	SaveDataToAsr("Filters", "Filter settings", &filter_char);

	string temp = "";

	for (int i = 1; i < 3; i++)
	{
		string prefix = "SRW" + std::to_string(i);

		temp = std::to_string(appWindows[i]->m_Area.left);
		SaveDataToAsr(string(prefix + "TopLeftX").c_str(), "SRW position", temp.c_str());

		temp = std::to_string(appWindows[i]->m_Area.top);
		SaveDataToAsr(string(prefix + "TopLeftY").c_str(), "SRW position", temp.c_str());

		temp = std::to_string(appWindows[i]->m_Area.right);
		SaveDataToAsr(string(prefix + "BottomRightX").c_str(), "SRW position", temp.c_str());

		temp = std::to_string(appWindows[i]->m_Area.bottom);
		SaveDataToAsr(string(prefix + "BottomRightY").c_str(), "SRW position", temp.c_str());

		temp = std::to_string(appWindows[i]->m_Offset.x);
		SaveDataToAsr(string(prefix + "OffsetX").c_str(), "SRW offset", temp.c_str());

		temp = std::to_string(appWindows[i]->m_Offset.y);
		SaveDataToAsr(string(prefix + "OffsetY").c_str(), "SRW offset", temp.c_str());

		temp = std::to_string(appWindows[i]->m_Filter);
		SaveDataToAsr(string(prefix + "Filter").c_str(), "SRW filter", temp.c_str());

		temp = std::to_string(appWindows[i]->m_Scale);
		SaveDataToAsr(string(prefix + "Scale").c_str(), "SRW range", temp.c_str());

		temp = std::to_string((int)appWindows[i]->m_Rotation);
		SaveDataToAsr(string(prefix + "Rotation").c_str(), "SRW rotation", temp.c_str());

		string to_save = "0";
		if (appWindowDisplays[i])
			to_save = "1";
		SaveDataToAsr(string(prefix + "Display").c_str(), "Display Secondary Radar Window", to_save.c_str());
	}
}

void CSMRRadar::OnMoveScreenObject(int ObjectType, const char* sObjectId, POINT Pt, RECT Area, bool Released)
{
	Logger::info(string(__FUNCSIG__));

	if (ObjectType == APPWINDOW_ONE || ObjectType == APPWINDOW_TWO)
	{
		const int appWindowId = ObjectType - APPWINDOW_BASE;

		if (const bool toggleCursor = appWindows[appWindowId]->OnMoveScreenObject(sObjectId, Pt, Area, Released); !
			toggleCursor)
		{
			if (standardCursor)
			{
				if (strcmp(sObjectId, "topbar") == 0)
					smrCursor = CopyCursor(
						(HCURSOR)::LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDC_SMRMOVEWINDOW), IMAGE_CURSOR, 0
							, 0, LR_SHARED));
				else if (strcmp(sObjectId, "resize") == 0)
					smrCursor = CopyCursor(
						(HCURSOR)::LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDC_SMRRESIZE), IMAGE_CURSOR, 0, 0,
							LR_SHARED));

				AFX_MANAGE_STATE(AfxGetStaticModuleState());
				ASSERT(smrCursor);
				SetCursor(smrCursor);
				standardCursor = false;
			}
		}
		else
		{
			if (!standardCursor)
			{
				if (customCursor)
				{
					smrCursor = CopyCursor(
						(HCURSOR)::LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDC_SMRCURSOR), IMAGE_CURSOR, 0, 0,
							LR_SHARED));
				}
				else
				{
					smrCursor = (HCURSOR)::LoadCursor(nullptr, IDC_ARROW);
				}

				AFX_MANAGE_STATE(AfxGetStaticModuleState());
				ASSERT(smrCursor);
				SetCursor(smrCursor);
				standardCursor = true;
			}
		}
	}

	if (ObjectType == DRAWING_TAG || ObjectType == TAG_CITEM_MANUALCORRELATE || ObjectType == TAG_CITEM_CALLSIGN ||
		ObjectType == TAG_CITEM_FPBOX || ObjectType == TAG_CITEM_RWY || ObjectType == TAG_CITEM_SID || ObjectType ==
		TAG_CITEM_GATE || ObjectType == TAG_CITEM_NO || ObjectType == TAG_CITEM_GROUNDSTATUS)
	{
		const CRadarTarget rt = GetPlugIn()->RadarTargetSelect(sObjectId);

		if (!Released)
		{
			if (standardCursor)
			{
				smrCursor = CopyCursor(
					(HCURSOR)::LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDC_SMRMOVETAG), IMAGE_CURSOR, 0, 0,
						LR_SHARED));
				AFX_MANAGE_STATE(AfxGetStaticModuleState());
				ASSERT(smrCursor);
				SetCursor(smrCursor);
				standardCursor = false;
			}
		}
		else
		{
			if (!standardCursor)
			{
				if (customCursor)
				{
					smrCursor = CopyCursor(
						(HCURSOR)::LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDC_SMRCURSOR), IMAGE_CURSOR, 0, 0,
							LR_SHARED));
				}
				else
				{
					smrCursor = (HCURSOR)::LoadCursor(nullptr, IDC_ARROW);
				}

				AFX_MANAGE_STATE(AfxGetStaticModuleState());
				ASSERT(smrCursor);
				SetCursor(smrCursor);
				standardCursor = true;
			}
		}

		if (rt.IsValid())
		{
			const CRect Temp = Area;
			const POINT TagCenterPix = Temp.TopLeft();
			const POINT AcPosPix = ConvertCoordFromPositionToPixel(
				GetPlugIn()->RadarTargetSelect(sObjectId).GetPosition().GetPosition());
			const POINT CustomTag = {TagCenterPix.x - AcPosPix.x, TagCenterPix.y - AcPosPix.y};

			double angle = RadToDeg(atan2(CustomTag.y, CustomTag.x));
			angle = fmod(angle + 360, 360);

			TagAngles[sObjectId] = angle;
			TagLeaderLineLength[sObjectId] = min(int(DistancePts(AcPosPix, TagCenterPix)),
			                                     LeaderLineDefaultlenght * 4);


			GetPlugIn()->SetASELAircraft(GetPlugIn()->FlightPlanSelect(sObjectId));

			if (Released)
			{
				TagBeingDragged = "";
			}
			else
			{
				TagBeingDragged = sObjectId;
			}

			RequestRefresh();
		}
	}

	if (ObjectType == RIMCAS_IAW)
	{
		TimePopupAreas[sObjectId] = Area;

		if (!Released)
		{
			if (standardCursor)
			{
				smrCursor = CopyCursor(
					(HCURSOR)::LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDC_SMRMOVEWINDOW), IMAGE_CURSOR, 0, 0,
						LR_SHARED));

				AFX_MANAGE_STATE(AfxGetStaticModuleState());
				ASSERT(smrCursor);
				SetCursor(smrCursor);
				standardCursor = false;
			}
		}
		else
		{
			if (!standardCursor)
			{
				if (customCursor)
				{
					smrCursor = CopyCursor(
						(HCURSOR)::LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDC_SMRCURSOR), IMAGE_CURSOR, 0, 0,
							LR_SHARED));
				}
				else
				{
					smrCursor = (HCURSOR)::LoadCursor(nullptr, IDC_ARROW);
				}

				AFX_MANAGE_STATE(AfxGetStaticModuleState());
				ASSERT(smrCursor);
				SetCursor(smrCursor);
				standardCursor = true;
			}
		}
	}

	mouseLocation = Pt;
	RequestRefresh();
}

void CSMRRadar::OnOverScreenObject(int ObjectType, const char* sObjectId, POINT Pt, RECT Area)
{
	Logger::info(string(__FUNCSIG__));
	mouseLocation = Pt;
	RequestRefresh();
}

void CSMRRadar::OnClickScreenObject(int ObjectType, const char* sObjectId, POINT mouseLocation, RECT Area, int Button)
{
	Logger::info(string(__FUNCSIG__));

	if (ObjectType == APPWINDOW_ONE || ObjectType == APPWINDOW_TWO)
	{
		int appWindowId = ObjectType - APPWINDOW_BASE;

		if (strcmp(sObjectId, "close") == 0)
			appWindowDisplays[appWindowId] = false;
		if (strcmp(sObjectId, "range") == 0)
		{
			GetPlugIn()->OpenPopupList(Area, "SRW Zoom", 1);
			GetPlugIn()->AddPopupListElement("55", "", RIMCAS_UPDATERANGE + appWindowId, false,
			                                 int(appWindows[appWindowId]->m_Scale == 55));
			GetPlugIn()->AddPopupListElement("50", "", RIMCAS_UPDATERANGE + appWindowId, false,
			                                 int(appWindows[appWindowId]->m_Scale == 50));
			GetPlugIn()->AddPopupListElement("45", "", RIMCAS_UPDATERANGE + appWindowId, false,
			                                 int(appWindows[appWindowId]->m_Scale == 45));
			GetPlugIn()->AddPopupListElement("40", "", RIMCAS_UPDATERANGE + appWindowId, false,
			                                 int(appWindows[appWindowId]->m_Scale == 40));
			GetPlugIn()->AddPopupListElement("35", "", RIMCAS_UPDATERANGE + appWindowId, false,
			                                 int(appWindows[appWindowId]->m_Scale == 35));
			GetPlugIn()->AddPopupListElement("30", "", RIMCAS_UPDATERANGE + appWindowId, false,
			                                 int(appWindows[appWindowId]->m_Scale == 30));
			GetPlugIn()->AddPopupListElement("25", "", RIMCAS_UPDATERANGE + appWindowId, false,
			                                 int(appWindows[appWindowId]->m_Scale == 25));
			GetPlugIn()->AddPopupListElement("20", "", RIMCAS_UPDATERANGE + appWindowId, false,
			                                 int(appWindows[appWindowId]->m_Scale == 20));
			GetPlugIn()->AddPopupListElement("15", "", RIMCAS_UPDATERANGE + appWindowId, false,
			                                 int(appWindows[appWindowId]->m_Scale == 15));
			GetPlugIn()->AddPopupListElement("10", "", RIMCAS_UPDATERANGE + appWindowId, false,
			                                 int(appWindows[appWindowId]->m_Scale == 10));
			GetPlugIn()->AddPopupListElement("5", "", RIMCAS_UPDATERANGE + appWindowId, false,
			                                 int(appWindows[appWindowId]->m_Scale == 5));
			GetPlugIn()->AddPopupListElement("1", "", RIMCAS_UPDATERANGE + appWindowId, false,
			                                 int(appWindows[appWindowId]->m_Scale == 1));
			GetPlugIn()->AddPopupListElement("Close", "", RIMCAS_CLOSE, false, 2, false, true);
		}
		if (strcmp(sObjectId, "filter") == 0)
		{
			GetPlugIn()->OpenPopupList(Area, "SRW Filter (ft)", 1);
			GetPlugIn()->AddPopupListElement("UNL", "", RIMCAS_UPDATEFILTER + appWindowId, false,
			                                 int(appWindows[appWindowId]->m_Filter == 66000));
			GetPlugIn()->AddPopupListElement("9500", "", RIMCAS_UPDATEFILTER + appWindowId, false,
			                                 int(appWindows[appWindowId]->m_Filter == 9500));
			GetPlugIn()->AddPopupListElement("8500", "", RIMCAS_UPDATEFILTER + appWindowId, false,
			                                 int(appWindows[appWindowId]->m_Filter == 8500));
			GetPlugIn()->AddPopupListElement("7500", "", RIMCAS_UPDATEFILTER + appWindowId, false,
			                                 int(appWindows[appWindowId]->m_Filter == 7500));
			GetPlugIn()->AddPopupListElement("6500", "", RIMCAS_UPDATEFILTER + appWindowId, false,
			                                 int(appWindows[appWindowId]->m_Filter == 6500));
			GetPlugIn()->AddPopupListElement("5500", "", RIMCAS_UPDATEFILTER + appWindowId, false,
			                                 int(appWindows[appWindowId]->m_Filter == 5500));
			GetPlugIn()->AddPopupListElement("4500", "", RIMCAS_UPDATEFILTER + appWindowId, false,
			                                 int(appWindows[appWindowId]->m_Filter == 4500));
			GetPlugIn()->AddPopupListElement("3500", "", RIMCAS_UPDATEFILTER + appWindowId, false,
			                                 int(appWindows[appWindowId]->m_Filter == 3500));
			GetPlugIn()->AddPopupListElement("2500", "", RIMCAS_UPDATEFILTER + appWindowId, false,
			                                 int(appWindows[appWindowId]->m_Filter == 2500));
			GetPlugIn()->AddPopupListElement("1500", "", RIMCAS_UPDATEFILTER + appWindowId, false,
			                                 int(appWindows[appWindowId]->m_Filter == 1500));
			GetPlugIn()->AddPopupListElement("500", "", RIMCAS_UPDATEFILTER + appWindowId, false,
			                                 int(appWindows[appWindowId]->m_Filter == 500));
			string tmp = std::to_string(GetPlugIn()->GetTransitionAltitude());
			GetPlugIn()->AddPopupListElement(tmp.c_str(), "", RIMCAS_UPDATEFILTER + appWindowId, false, 2, false, true);
			GetPlugIn()->AddPopupListElement("Close", "", RIMCAS_CLOSE, false, 2, false, true);
		}
		if (strcmp(sObjectId, "rotate") == 0)
		{
			GetPlugIn()->OpenPopupList(Area, "SRW Rotate (deg)", 1);
			for (int k = 0; k <= 360; k++)
			{
				string tmp = std::to_string(k);
				GetPlugIn()->AddPopupListElement(tmp.c_str(), "", RIMCAS_UPDATEROTATE + appWindowId, false,
				                                 int(appWindows[appWindowId]->m_Rotation == k));
			}
			GetPlugIn()->AddPopupListElement("Close", "", RIMCAS_CLOSE, false, 2, false, true);
		}
	}

	if (ObjectType == RIMCAS_ACTIVE_AIRPORT)
	{
		GetPlugIn()->OpenPopupEdit(Area, RIMCAS_ACTIVE_AIRPORT_FUNC, getActiveAirport().c_str());
	}

	if (ObjectType == DRAWING_BACKGROUND_CLICK)
	{
		if (QDMSelectEnabled)
		{
			if (Button == BUTTON_LEFT)
			{
				QDMSelectPt = mouseLocation;
				RequestRefresh();
			}

			if (Button == BUTTON_RIGHT)
			{
				QDMSelectEnabled = false;
				RequestRefresh();
			}
		}

		if (QDMenabled)
		{
			if (Button == BUTTON_RIGHT)
			{
				QDMenabled = false;
				RequestRefresh();
			}
		}

		if (this->context_menu_for.has_value())
		{
			this->context_menu_for = std::nullopt;
			this->RequestRefresh();
		}
	}

	if (ObjectType == RIMCAS_MENU)
	{
		if (strcmp(sObjectId, "DisplayMenu") == 0)
		{
			Area.top = Area.top + 30;
			Area.bottom = Area.bottom + 30;

			GetPlugIn()->OpenPopupList(Area, "Display Menu", 1);
			GetPlugIn()->AddPopupListElement("QDR Fixed Reference", "", RIMCAS_QDM_TOGGLE);
			GetPlugIn()->AddPopupListElement("QDR Select Reference", "", RIMCAS_QDM_SELECT_TOGGLE);
			GetPlugIn()->AddPopupListElement("SRW 1", "", APPWINDOW_ONE, false, int(appWindowDisplays[1]));
			GetPlugIn()->AddPopupListElement("SRW 2", "", APPWINDOW_TWO, false, int(appWindowDisplays[2]));
			GetPlugIn()->AddPopupListElement("Profiles", "", RIMCAS_OPEN_LIST);
			GetPlugIn()->AddPopupListElement("Tag Error Lines", "", RIMCAS_ERR_LINE_TOGGLE, false,
			                                 show_err_lines ? POPUP_ELEMENT_CHECKED : POPUP_ELEMENT_UNCHECKED);
			GetPlugIn()->AddPopupListElement("Shift Top Bar", "", RIMCAS_SHIFT_TOP_BAR_TOGGLE, false,
			                                 shift_top_bar ? POPUP_ELEMENT_CHECKED : POPUP_ELEMENT_UNCHECKED);
			GetPlugIn()->AddPopupListElement("Close", "", RIMCAS_CLOSE, false, 2, false, true);
		}

		if (strcmp(sObjectId, "FilterMenu") == 0)
		{
			Area.top = Area.top + 30;
			Area.bottom = Area.bottom + 30;
			GetPlugIn()->OpenPopupList(Area, "Filter Menu", 1);
			GetPlugIn()->AddPopupListElement("Show Free", "", FILTER_SHOW_FREE, false,
			                                 filters.show_free ? POPUP_ELEMENT_CHECKED : POPUP_ELEMENT_UNCHECKED);
			GetPlugIn()->AddPopupListElement("Show Not Mine", "", FILTER_NON_ASSUMED, false,
			                                 filters.show_nonmine ? POPUP_ELEMENT_CHECKED : POPUP_ELEMENT_UNCHECKED);
			GetPlugIn()->AddPopupListElement("Show On Blocks", "", FILTER_SHOW_ON_BLOCKS, false,
			                                 filters.show_on_blocks ? POPUP_ELEMENT_CHECKED : POPUP_ELEMENT_UNCHECKED);
			GetPlugIn()->AddPopupListElement("Show NSTS", "", FILTER_SHOW_NSTS, false,
			                                 filters.show_nsts ? POPUP_ELEMENT_CHECKED : POPUP_ELEMENT_UNCHECKED);
			GetPlugIn()->AddPopupListElement("Show STUP", "", FILTER_SHOW_STUP, false,
			                                 filters.show_stup ? POPUP_ELEMENT_CHECKED : POPUP_ELEMENT_UNCHECKED);
			GetPlugIn()->AddPopupListElement("Close", "", RIMCAS_CLOSE, false, POPUP_ELEMENT_NO_CHECKBOX, false, true);
		}

		if (strcmp(sObjectId, "BeluxMenu") == 0)
		{
			Area.top = Area.top + 30;
			Area.bottom = Area.bottom + 30;
			GetPlugIn()->OpenPopupList(Area, "Belux Menu", 1);
			GetPlugIn()->AddPopupListElement("Belux pro mode", "", RIMCAS_OPEN_LIST);
		}


		if (strcmp(sObjectId, "TargetMenu") == 0)
		{
			Area.top = Area.top + 30;
			Area.bottom = Area.bottom + 30;

			GetPlugIn()->OpenPopupList(Area, "Target", 1);
			GetPlugIn()->AddPopupListElement("Label Font Size", "", RIMCAS_OPEN_LIST);
			GetPlugIn()->AddPopupListElement("Afterglow", "", RIMCAS_UPDATE_AFTERGLOW, false, int(Afterglow));
			GetPlugIn()->AddPopupListElement("GRND Trail Dots", "", RIMCAS_OPEN_LIST);
			GetPlugIn()->AddPopupListElement("APPR Trail Dots", "", RIMCAS_OPEN_LIST);
			GetPlugIn()->AddPopupListElement("Predicted Track Line", "", RIMCAS_OPEN_LIST);
			GetPlugIn()->AddPopupListElement("Acquire", "", RIMCAS_UPDATE_ACQUIRE);
			GetPlugIn()->AddPopupListElement("Drop", "", RIMCAS_UPDATE_RELEASE);
			GetPlugIn()->AddPopupListElement("Close", "", RIMCAS_CLOSE, false, 2, false, true);
		}

		if (strcmp(sObjectId, "MapMenu") == 0)
		{
			Area.top = Area.top + 30;
			Area.bottom = Area.bottom + 30;

			GetPlugIn()->OpenPopupList(Area, "Maps", 1);
			GetPlugIn()->AddPopupListElement("Airport Maps", "", RIMCAS_OPEN_LIST);
			GetPlugIn()->AddPopupListElement("Custom Maps", "", RIMCAS_OPEN_LIST);
			GetPlugIn()->AddPopupListElement("Close", "", RIMCAS_CLOSE, false, 2, false, true);
		}

		if (strcmp(sObjectId, "ColourMenu") == 0)
		{
			Area.top = Area.top + 30;
			Area.bottom = Area.bottom + 30;

			GetPlugIn()->OpenPopupList(Area, "Colours", 1);
			GetPlugIn()->AddPopupListElement("Colour Settings", "", RIMCAS_OPEN_LIST);
			GetPlugIn()->AddPopupListElement("Brightness", "", RIMCAS_OPEN_LIST);
			GetPlugIn()->AddPopupListElement("Close", "", RIMCAS_CLOSE, false, 2, false, true);
		}

		if (strcmp(sObjectId, "RIMCASMenu") == 0)
		{
			Area.top = Area.top + 30;
			Area.bottom = Area.bottom + 30;

			GetPlugIn()->OpenPopupList(Area, "Alerts", 1);
			GetPlugIn()->AddPopupListElement("Conflict Alert ARR", "", RIMCAS_OPEN_LIST);
			GetPlugIn()->AddPopupListElement("Conflict Alert DEP", "", RIMCAS_OPEN_LIST);
			GetPlugIn()->AddPopupListElement("Runway closed", "", RIMCAS_OPEN_LIST);
			GetPlugIn()->AddPopupListElement("Visibility", "", RIMCAS_OPEN_LIST);
			GetPlugIn()->AddPopupListElement("Close", "", RIMCAS_CLOSE, false, 2, false, true);
		}

		if (strcmp(sObjectId, "/") == 0)
		{
			if (Button == BUTTON_LEFT)
			{
				DistanceToolActive = !DistanceToolActive;
				if (!DistanceToolActive)
					ActiveDistance = pair<string, string>("", "");

				if (DistanceToolActive)
				{
					QDMenabled = false;
					QDMSelectEnabled = false;
				}
			}
			if (Button == BUTTON_RIGHT)
			{
				DistanceToolActive = false;
				ActiveDistance = pair<string, string>("", "");
				DistanceTools.clear();
			}
		}
	}

	if (ObjectType == DRAWING_TAG || ObjectType == DRAWING_AC_SYMBOL)
	{
		CRadarTarget rt = GetPlugIn()->RadarTargetSelect(sObjectId);
		//GetPlugIn()->SetASELAircraft(rt); // NOTE: This does NOT work eventhough the api says it should?
		GetPlugIn()->SetASELAircraft(GetPlugIn()->FlightPlanSelect(sObjectId));
		// make sure the correct aircraft is selected before calling 'StartTagFunction'

		if (rt.GetCorrelatedFlightPlan().IsValid())
		{
			StartTagFunction(rt.GetCallsign(), nullptr, EuroScopePlugIn::TAG_ITEM_TYPE_CALLSIGN, rt.GetCallsign(),
			                 nullptr, EuroScopePlugIn::TAG_ITEM_FUNCTION_NO, mouseLocation, Area);
		}

		// Release & correlate actions

		if (ReleaseInProgress || AcquireInProgress)
		{
			if (ReleaseInProgress)
			{
				this->manually_release(rt.GetSystemID());
			}

			if (AcquireInProgress)
			{
				this->manually_correlate(rt.GetSystemID());
			}


			CorrelateCursor();

			return;
		}

		if (ObjectType == DRAWING_AC_SYMBOL)
		{
			if (QDMSelectEnabled)
			{
				if (Button == BUTTON_LEFT)
				{
					QDMSelectPt = mouseLocation;
					RequestRefresh();
				}
			}
			else if (DistanceToolActive)
			{
				if (ActiveDistance.first == "")
				{
					ActiveDistance.first = sObjectId;
				}
				else if (ActiveDistance.second == "")
				{
					ActiveDistance.second = sObjectId;
					DistanceTools.insert(ActiveDistance);
					ActiveDistance = pair<string, string>("", "");
					DistanceToolActive = false;
				}
				RequestRefresh();
			}
			else
			{
				if (Button == BUTTON_LEFT)
				{
					if (TagAngles.find(sObjectId) == TagAngles.end())
					{
						TagAngles[sObjectId] = 0;
					}
					else
					{
						TagAngles[sObjectId] = fmod(TagAngles[sObjectId] - 22.5, 360);
					}
				}

				if (Button == BUTTON_RIGHT)
				{
					context_menu_for = ContextMenuData{rt.GetSystemID(), rt.GetCallsign()};
					context_menu_pos = mouseLocation;
					//if (TagAngles.find(sObjectId) == TagAngles.end())
					//{
					//	TagAngles[sObjectId] = 0;
					//}
					//else
					//{
					//	TagAngles[sObjectId] = fmod(TagAngles[sObjectId] + 22.5, 360);
					//}
				}

				RequestRefresh();
			}
		}
	}

	if (ObjectType == DRAWING_AC_SYMBOL_APPWINDOW1 || ObjectType == DRAWING_AC_SYMBOL_APPWINDOW2)
	{
		if (DistanceToolActive)
		{
			if (ActiveDistance.first == "")
			{
				ActiveDistance.first = sObjectId;
			}
			else if (ActiveDistance.second == "")
			{
				ActiveDistance.second = sObjectId;
				DistanceTools.insert(ActiveDistance);
				ActiveDistance = pair<string, string>("", "");
				DistanceToolActive = false;
			}
			RequestRefresh();
		}
		else if (Button == BUTTON_RIGHT)
		{
			CRadarTarget rt = GetPlugIn()->RadarTargetSelect(sObjectId);
			GetPlugIn()->SetASELAircraft(GetPlugIn()->FlightPlanSelect(sObjectId));
			context_menu_for = ContextMenuData{rt.GetSystemID(), rt.GetCallsign()};
			context_menu_pos = mouseLocation;
			RequestRefresh();
		}
		else
		{
			if (ObjectType == DRAWING_AC_SYMBOL_APPWINDOW1)
				appWindows[1]->OnClickScreenObject(sObjectId, mouseLocation, Button);

			if (ObjectType == DRAWING_AC_SYMBOL_APPWINDOW2)
				appWindows[2]->OnClickScreenObject(sObjectId, mouseLocation, Button);
		}
	}

	map<const int, const int> TagObjectMiddleTypes = {
		{TAG_CITEM_CALLSIGN, TAG_ITEM_FUNCTION_COMMUNICATION_POPUP},
	};

	map<const int, const int> TagObjectRightTypes = {
		{TAG_CITEM_CALLSIGN, TAG_ITEM_FUNCTION_HANDOFF_POPUP_MENU},
		{TAG_CITEM_FPBOX, TAG_ITEM_FUNCTION_OPEN_FP_DIALOG},
		{TAG_CITEM_RWY, TAG_ITEM_FUNCTION_ASSIGNED_RUNWAY},
		{TAG_CITEM_SID, TAG_ITEM_FUNCTION_ASSIGNED_SID},
		{TAG_CITEM_GATE, TAG_ITEM_FUNCTION_EDIT_SCRATCH_PAD},
		{TAG_CITEM_GROUNDSTATUS, TAG_ITEM_FUNCTION_SET_GROUND_STATUS}
	};

	if (Button == BUTTON_LEFT)
	{
		CRadarTarget rt = GetPlugIn()->RadarTargetSelect(sObjectId);
		GetPlugIn()->SetASELAircraft(GetPlugIn()->FlightPlanSelect(sObjectId));
		if (rt.GetCorrelatedFlightPlan().IsValid())
		{
			StartTagFunction(rt.GetCallsign(), nullptr, TAG_ITEM_TYPE_CALLSIGN, rt.GetCallsign(), nullptr,
			                 TAG_ITEM_FUNCTION_NO, mouseLocation, Area);
		}
	}

	if (Button == BUTTON_MIDDLE && TagObjectMiddleTypes[ObjectType])
	{
		int TagMenu = TagObjectMiddleTypes[ObjectType];
		CRadarTarget rt = GetPlugIn()->RadarTargetSelect(sObjectId);
		GetPlugIn()->SetASELAircraft(GetPlugIn()->FlightPlanSelect(sObjectId));
		StartTagFunction(rt.GetCallsign(), nullptr, EuroScopePlugIn::TAG_ITEM_TYPE_CALLSIGN, rt.GetCallsign(), nullptr,
		                 TagMenu, mouseLocation, Area);
	}

	if (Button == BUTTON_RIGHT && TagObjectRightTypes[ObjectType])
	{
		int TagMenu = TagObjectRightTypes[ObjectType];
		CRadarTarget rt = GetPlugIn()->RadarTargetSelect(sObjectId);
		GetPlugIn()->SetASELAircraft(GetPlugIn()->FlightPlanSelect(sObjectId));
		StartTagFunction(rt.GetCallsign(), nullptr, EuroScopePlugIn::TAG_ITEM_TYPE_CALLSIGN, rt.GetCallsign(), nullptr,
		                 TagMenu, mouseLocation, Area);
	}

	if (ObjectType == RIMCAS_DISTANCE_TOOL)
	{
		vector<string> s = split(sObjectId, ',');
		pair<string, string> toRemove = pair<string, string>(s.front(), s.back());

		typedef multimap<string, string>::iterator iterator;
		std::pair<iterator, iterator> iterpair = DistanceTools.equal_range(toRemove.first);

		for (auto it = iterpair.first; it != iterpair.second; ++it)
		{
			if (it->second == toRemove.second)
			{
				it = DistanceTools.erase(it);
				break;
			}
		}
	}

	if (ObjectType == CONTEXT_RELEASE)
	{
		this->manually_release(sObjectId);
		this->context_menu_for = std::nullopt;
	}

	if (ObjectType == CONTEXT_ACQUIRE)
	{
		this->manually_correlate(sObjectId);
		this->context_menu_for = std::nullopt;
	}

	if (ObjectType == CONTEXT_LAND)
	{
		const auto rt = GetPlugIn()->RadarTargetSelectASEL();
		if (strcmp(rt.GetSystemID(), sObjectId) == 0)
		{
			std::string strip_seven = rt.GetCorrelatedFlightPlan().GetControllerAssignedData().
			                             GetFlightStripAnnotation(7);
			const bool cleared = strip_seven.find("K") != std::string::npos || strip_seven.find("R") !=
				std::string::npos;
			if (cleared)
			{
				replaceAll(strip_seven, "K", "");
				replaceAll(strip_seven, "R", "");
				rt.GetCorrelatedFlightPlan().GetControllerAssignedData().SetFlightStripAnnotation(
					7, strip_seven.c_str());
			}
			else
			{
				strip_seven.push_back('K');
				rt.GetCorrelatedFlightPlan().GetControllerAssignedData().SetFlightStripAnnotation(
					7, strip_seven.c_str());
			}
		}
	}

	RequestRefresh();
};

void CSMRRadar::OnFunctionCall(int FunctionId, const char* sItemString, POINT Pt, RECT Area)
{
	Logger::info(string(__FUNCSIG__));
	mouseLocation = Pt;
	if (FunctionId == APPWINDOW_ONE || FunctionId == APPWINDOW_TWO)
	{
		const int id = FunctionId - APPWINDOW_BASE;
		appWindowDisplays[id] = !appWindowDisplays[id];
	}

	if (FunctionId == BELUX_SETPROMODE)
	{
		belux_promode = !belux_promode;
	}
	if (FunctionId == BELUX_SETPROMODEEASY)
	{
		belux_promode_easy = !belux_promode_easy;
	}

	if (FunctionId == RIMCAS_ACTIVE_AIRPORT_FUNC)
	{
		setActiveAirport(sItemString);
		SaveDataToAsr("Airport", "Active airport", getActiveAirport().c_str());
	}

	if (FunctionId == RIMCAS_UPDATE_FONTS)
	{
		if (strcmp(sItemString, "Size 1") == 0)
			currentFontSize = 1;
		if (strcmp(sItemString, "Size 2") == 0)
			currentFontSize = 2;
		if (strcmp(sItemString, "Size 3") == 0)
			currentFontSize = 3;
		if (strcmp(sItemString, "Size 4") == 0)
			currentFontSize = 4;
		if (strcmp(sItemString, "Size 5") == 0)
			currentFontSize = 5;

		ShowLists["Label Font Size"] = true;
	}

	if (FunctionId == RIMCAS_QDM_TOGGLE)
	{
		QDMenabled = !QDMenabled;
		QDMSelectEnabled = false;
	}

	if (FunctionId == RIMCAS_ERR_LINE_TOGGLE)
	{
		show_err_lines = !show_err_lines;
	}

	if (FunctionId == RIMCAS_SHIFT_TOP_BAR_TOGGLE)
	{
		shift_top_bar = !shift_top_bar;
	}

	if (FunctionId == RIMCAS_QDM_SELECT_TOGGLE)
	{
		if (!QDMSelectEnabled)
		{
			QDMSelectPt = ConvertCoordFromPositionToPixel(AirportPositions[getActiveAirport()]);
		}
		QDMSelectEnabled = !QDMSelectEnabled;
		QDMenabled = false;
	}

	if (FunctionId == RIMCAS_UPDATE_PROFILE)
	{
		this->CSMRRadar::LoadProfile(sItemString);
		LoadCustomFont();
		SaveDataToAsr("ActiveProfile", "vSMR active profile", sItemString);

		ShowLists["Profiles"] = true;
	}

	if (FunctionId == RIMCAS_UPDATEFILTER1 || FunctionId == RIMCAS_UPDATEFILTER2)
	{
		const int id = FunctionId - RIMCAS_UPDATEFILTER;
		if (startsWith("UNL", sItemString))
			sItemString = "66000";
		appWindows[id]->m_Filter = atoi(sItemString);
	}

	if (FunctionId == RIMCAS_UPDATERANGE1 || FunctionId == RIMCAS_UPDATERANGE2)
	{
		const int id = FunctionId - RIMCAS_UPDATERANGE;
		appWindows[id]->m_Scale = atoi(sItemString);
	}

	if (FunctionId == RIMCAS_UPDATEROTATE1 || FunctionId == RIMCAS_UPDATEROTATE2)
	{
		const int id = FunctionId - RIMCAS_UPDATEROTATE;
		appWindows[id]->m_Rotation = atoi(sItemString);
	}

	if (FunctionId == RIMCAS_UPDATE_BRIGHNESS)
	{
		if (strcmp(sItemString, "Day") == 0)
			ColorSettingsDay = true;
		else
			ColorSettingsDay = false;

		ShowLists["Colour Settings"] = true;

		RequestRefresh();
	}

	if (FunctionId == RIMCAS_CA_ARRIVAL_FUNC)
	{
		RimcasInstance->toggleMonitoredRunwayArr(string(sItemString));

		ShowLists["Conflict Alert ARR"] = true;

		RequestRefresh();
	}

	if (FunctionId == RIMCAS_CA_MONITOR_FUNC)
	{
		RimcasInstance->toggleMonitoredRunwayDep(string(sItemString));

		ShowLists["Conflict Alert DEP"] = true;

		RequestRefresh();
	}

	if (FunctionId == RIMCAS_CLOSED_RUNWAYS_FUNC)
	{
		RimcasInstance->toggleClosedRunway(string(sItemString));

		ShowLists["Runway closed"] = true;

		RequestRefresh();
	}

	if (FunctionId == RIMCAS_OPEN_LIST)
	{
		ShowLists[string(sItemString)] = true;
		ListAreas[string(sItemString)] = Area;

		RequestRefresh();
	}

	if (FunctionId == RIMCAS_UPDATE_LVP)
	{
		if (strcmp(sItemString, "Normal") == 0)
			isLVP = false;
		if (strcmp(sItemString, "Low") == 0)
			isLVP = true;

		ShowLists["Visibility"] = true;

		RequestRefresh();
	}

	if (FunctionId == RIMCAS_UPDATE_AFTERGLOW)
	{
		Afterglow = !Afterglow;
	}

	if (FunctionId == RIMCAS_UPDATE_GND_TRAIL)
	{
		Trail_Gnd = atoi(sItemString);

		ShowLists["GRND Trail Dots"] = true;
	}

	if (FunctionId == RIMCAS_UPDATE_APP_TRAIL)
	{
		Trail_App = atoi(sItemString);

		ShowLists["APPR Trail Dots"] = true;
	}

	if (FunctionId == RIMCAS_UPDATE_PTL)
	{
		PredictedLength = atoi(sItemString);

		ShowLists["Predicted Track Line"] = true;
	}

	if (FunctionId == RIMCAS_BRIGHTNESS_LABEL)
	{
		ColorManager->update_brightness("label", std::atoi(sItemString));
		ShowLists["Label"] = true;
	}

	if (FunctionId == RIMCAS_BRIGHTNESS_AFTERGLOW)
	{
		ColorManager->update_brightness("afterglow", std::atoi(sItemString));
		ShowLists["Afterglow"] = true;
	}

	if (FunctionId == RIMCAS_BRIGHTNESS_SYMBOL)
	{
		ColorManager->update_brightness("symbol", std::atoi(sItemString));
		ShowLists["Symbol"] = true;
	}

	if (FunctionId == RIMCAS_UPDATE_RELEASE)
	{
		ReleaseInProgress = !ReleaseInProgress;
		if (ReleaseInProgress)
			AcquireInProgress = false;
		NeedCorrelateCursor = ReleaseInProgress;

		CorrelateCursor();
	}

	if (FunctionId == RIMCAS_UPDATE_ACQUIRE)
	{
		AcquireInProgress = !AcquireInProgress;
		if (AcquireInProgress)
			ReleaseInProgress = false;
		NeedCorrelateCursor = AcquireInProgress;

		CorrelateCursor();
	}

	if (FunctionId == FILTER_SHOW_FREE)
	{
		filters.show_free = !filters.show_free;
	}

	if (FunctionId == FILTER_NON_ASSUMED)
	{
		filters.show_nonmine = !filters.show_nonmine;
	}

	if (FunctionId == FILTER_SHOW_ON_BLOCKS)
	{
		filters.show_on_blocks = !filters.show_on_blocks;
	}

	if (FunctionId == FILTER_SHOW_NSTS)
	{
		filters.show_nsts = !filters.show_nsts;
	}

	if (FunctionId == FILTER_SHOW_STUP)
	{
		filters.show_stup = !filters.show_stup;
	}
}

void CSMRRadar::RefreshAirportActivity(void)
{
	Logger::info(string(__FUNCSIG__));
	//
	// Getting the depatures and arrivals airports
	//

	Active_Arrivals.clear();
	for (CSectorElement airport = GetPlugIn()->SectorFileElementSelectFirst(SECTOR_ELEMENT_AIRPORT);
	     airport.IsValid();
	     airport = GetPlugIn()->SectorFileElementSelectNext(airport, SECTOR_ELEMENT_AIRPORT))
	{
		if (airport.IsElementActive(false))
		{
			string s = airport.GetName();
			s = s.substr(0, 4);
			transform(s.begin(), s.end(), s.begin(), ::toupper);
			Active_Arrivals.push_back(s);
		}
	}
}

void CSMRRadar::OnRadarTargetPositionUpdate(CRadarTarget RadarTarget)
{
}

string CSMRRadar::GetBottomLine(const char* Callsign)
{
	Logger::info(string(__FUNCSIG__));

	CFlightPlan fp = GetPlugIn()->FlightPlanSelect(Callsign);
	string to_render = "";
	if (fp.IsValid())
	{
		to_render += fp.GetCallsign();

		string callsign_code = fp.GetCallsign();
		callsign_code = callsign_code.substr(0, 3);
		to_render += " (" + Callsigns->getCallsign(callsign_code) + ")";

		to_render += " (";
		to_render += fp.GetPilotName();
		to_render += "): ";
		to_render += fp.GetFlightPlanData().GetAircraftFPType();
		to_render += " ";

		if (fp.GetFlightPlanData().IsReceived())
		{
			const char* assr = fp.GetControllerAssignedData().GetSquawk();
			const char* ssr = GetPlugIn()->RadarTargetSelect(fp.GetCallsign()).GetPosition().GetSquawk();
			if (strlen(assr) != 0 && !startsWith(ssr, assr))
			{
				to_render += assr;
				to_render += ":";
				to_render += ssr;
			}
			else
			{
				to_render += "I:";
				to_render += ssr;
			}

			to_render += " ";
			to_render += fp.GetFlightPlanData().GetOrigin();
			to_render += "==>";
			to_render += fp.GetFlightPlanData().GetDestination();
			to_render += " (";
			to_render += fp.GetFlightPlanData().GetAlternate();
			to_render += ")";

			to_render += " at ";
			int rfl = fp.GetControllerAssignedData().GetFinalAltitude();
			string rfl_s;
			if (rfl == 0)
				rfl = fp.GetFlightPlanData().GetFinalAltitude();
			if (rfl > GetPlugIn()->GetTransitionAltitude())
				rfl_s = "FL" + std::to_string(rfl / 100);
			else
				rfl_s = std::to_string(rfl) + "ft";

			to_render += rfl_s;
			to_render += " Route: ";
			to_render += fp.GetFlightPlanData().GetRoute();
		}
	}

	return to_render;
}

bool CSMRRadar::OnCompileCommand(const char* sCommandLine)
{
	Logger::info(string(__FUNCSIG__));
	if (strcmp(sCommandLine, ".smr reload") == 0)
	{
		CurrentConfig = new CConfig(ConfigPath);
		LoadProfile(CurrentConfig->getActiveProfileName());
		return true;
	}

	if (strcmp(sCommandLine, ".smr shift-top-bar") == 0)
	{
		shift_top_bar = !shift_top_bar;
		return true;
	}

	if (startsWith(".smr alt-mode ", sCommandLine))
	{
		const char* substring = &sCommandLine[14];
		alt_mode_keycode = atoi(substring);
		return true;
	}

	return false;
}

map<string, string> CSMRRadar::GenerateTagData(CRadarTarget rt, CFlightPlan fp, bool isAcCorrelated, bool isProMode,
                                               int TransitionAltitude, bool useSpeedForGates, string ActiveAirport)
{
	Logger::info(string(__FUNCSIG__));
	// ----
	// Tag items available
	// callsign: Callsign with freq state and comm *
	// actype: Aircraft type *
	// sctype: Aircraft type that changes for squawk error *
	// sqerror: Squawk error if there is one, or empty *
	// deprwy: Departure runway *
	// seprwy: Departure runway that changes to speed if speed > 25kts *
	// arvrwy: Arrival runway *
	// srvrwy: Speed that changes to arrival runway if speed < 25kts *
	// gate: Gate, from speed or scratchpad *
	// sate: Gate, from speed or scratchpad that changes to speed if speed > 25kts *
	// flightlevel: Flightlevel/Pressure altitude of the ac *
	// gs: Ground speed of the ac *
	// tendency: Climbing or descending symbol *
	// wake: Wake turbulance cat *
	// groundstatus: Current status *
	// ssr: the current squawk of the ac
	// sid: the assigned SID
	// ssid: a short version of the SID
	// origin: origin aerodrome
	// dest: destination aerodrome
	// ----

	bool IsPrimary = !rt.GetPosition().GetTransponderC();
	bool isAirborne = rt.GetPosition().GetReportedGS() > 50;

	// ----- Callsign -------
	string callsign = rt.GetCallsign();
	if (fp.IsValid())
	{
		if (fp.GetControllerAssignedData().GetCommunicationType() == 't' ||
			fp.GetControllerAssignedData().GetCommunicationType() == 'T' ||
			fp.GetControllerAssignedData().GetCommunicationType() == 'r' ||
			fp.GetControllerAssignedData().GetCommunicationType() == 'R' ||
			fp.GetControllerAssignedData().GetCommunicationType() == 'v' ||
			fp.GetControllerAssignedData().GetCommunicationType() == 'V')
		{
			if (fp.GetControllerAssignedData().GetCommunicationType() != 'v' &&
				fp.GetControllerAssignedData().GetCommunicationType() != 'V')
			{
				callsign.append("/");
				callsign += fp.GetControllerAssignedData().GetCommunicationType();
			}
		}
		else if (fp.GetFlightPlanData().GetCommunicationType() == 't' ||
			fp.GetFlightPlanData().GetCommunicationType() == 'r' ||
			fp.GetFlightPlanData().GetCommunicationType() == 'T' ||
			fp.GetFlightPlanData().GetCommunicationType() == 'R')
		{
			callsign.append("/");
			callsign += fp.GetFlightPlanData().GetCommunicationType();
		}

		switch (fp.GetState())
		{
		case FLIGHT_PLAN_STATE_TRANSFER_TO_ME_INITIATED:
			callsign = ">>" + callsign;
			break;

		case FLIGHT_PLAN_STATE_TRANSFER_FROM_ME_INITIATED:
			callsign = callsign + ">>";
			break;

		case FLIGHT_PLAN_STATE_ASSUMED:
		default:
			// Don't change callsign
			break;
		}

		if (strcmp(fp.GetGroundState(), "DEPA") == 0)
		{
			callsign += "^"; // Forms an up arrow in Euroscope font
		}
		// This uses the TopSky "Mark" _or_ Freq feature
		const string strip_seven = fp.GetControllerAssignedData().GetFlightStripAnnotation(7);
		if (
			strip_seven.find("K") != string::npos // K: TopSky Mark
			|| strip_seven.find("R") != string::npos // R: TopSky Freq
		)
		{
			callsign += "|"; // Forms a down arrow in Euroscope font, as in cleared to land
		}
	}

	// ----- Squawk error -------
	string sqerror = "";
	const char* assr = fp.GetControllerAssignedData().GetSquawk();
	const char* ssr = rt.GetPosition().GetSquawk();
	bool has_squawk_error = false;
	if (strlen(assr) != 0 && !startsWith(ssr, assr))
	{
		has_squawk_error = true;
		sqerror = "A";
		sqerror.append(assr);
	}

	// ----- Aircraft type -------

	string actype = "NoFPL";
	if (fp.IsValid() && fp.GetFlightPlanData().IsReceived())
		actype = fp.GetFlightPlanData().GetAircraftFPType();
	if (actype.size() > 4 && actype != "NoFPL")
		actype = actype.substr(0, 4);

	// ----- Aircraft type that changes to squawk error -------
	string sctype = actype;
	if (has_squawk_error)
		sctype = sqerror;

	// ----- Groundspeed -------
	string speed = std::to_string(rt.GetPosition().GetReportedGS());

	// ----- Departure runway -------
	string deprwy = fp.GetFlightPlanData().GetDepartureRwy();
	if (deprwy.length() == 0)
		deprwy = "RWY";

	// ----- Departure runway that changes for overspeed -------
	string seprwy = deprwy;
	if (rt.GetPosition().GetReportedGS() > 50)
		seprwy = std::to_string(rt.GetPosition().GetReportedGS());

	// ----- Arrival runway -------
	string arvrwy = fp.GetFlightPlanData().GetArrivalRwy();
	if (arvrwy.length() == 0)
		arvrwy = "RWY";

	// ----- Speed that changes to arrival runway -----
	string srvrwy = speed;
	if (rt.GetPosition().GetReportedGS() < 50)
		srvrwy = arvrwy;

	// ----- Gate -------
	string gate;
	gate = fp.GetControllerAssignedData().GetFlightStripAnnotation(4);
	if (gate.size() == 0 || gate == "0" || !isAcCorrelated)
		gate = "N/G";

	// ----- Gate that changes to speed -------
	string sate = gate;
	if (rt.GetPosition().GetReportedGS() > 50)
		sate = speed;

	// ----- Flightlevel -------
	int fl = rt.GetPosition().GetFlightLevel();
	int padding = 5;
	string pfls = "";
	if (fl <= TransitionAltitude)
	{
		fl = rt.GetPosition().GetPressureAltitude();
		pfls = "A";
		padding = 4;
	}
	string flightlevel = (pfls + padWithZeros(padding, fl)).substr(0, 3);

	// ----- Tendency -------
	string tendency = "-";
	int delta_fl = rt.GetPosition().GetFlightLevel() - rt.GetPreviousPosition(rt.GetPosition()).GetFlightLevel();
	if (abs(delta_fl) >= 50)
	{
		if (delta_fl < 0)
		{
			tendency = "|";
		}
		else
		{
			tendency = "^";
		}
	}

	// ----- Wake cat -------
	string wake = "?";
	if (fp.IsValid() && isAcCorrelated)
	{
		wake = "";
		wake += fp.GetFlightPlanData().GetAircraftWtc();
	}

	// ----- SSR -------
	string tssr = "";
	if (rt.IsValid())
	{
		tssr = rt.GetPosition().GetSquawk();
	}

	// ----- SID -------
	string dep = "SID";
	if (fp.IsValid() && isAcCorrelated)
	{
		dep = fp.GetFlightPlanData().GetSidName();
	}

	// ----- Short SID -------
	string ssid = dep;
	if (fp.IsValid() && ssid.size() > 5 && isAcCorrelated)
	{
		ssid = dep.substr(0, 3);
		ssid += dep.substr(dep.size() - 2, dep.size());
	}

	// ------- Origin aerodrome -------
	string origin = "????";
	if (isAcCorrelated)
	{
		origin = fp.GetFlightPlanData().GetOrigin();
	}

	// ------- Destination aerodrome -------
	string dest = "????";
	if (isAcCorrelated)
	{
		dest = fp.GetFlightPlanData().GetDestination();
	}


	string scratch_pad = fp.GetControllerAssignedData().GetScratchPadString();

	// ----- GSTAT -------
	string gstat = "STS";
	if (fp.IsValid() && isAcCorrelated)
	{
		if (strlen(fp.GetGroundState()) != 0)
			gstat = fp.GetGroundState();
	}

	// ----- Generating the replacing map -----
	map<string, string> TagReplacingMap;

	// System ID for uncorrelated
	TagReplacingMap["systemid"] = "T:";
	string tpss = rt.GetSystemID();
	TagReplacingMap["systemid"].append(tpss.substr(1, 6));

	// Pro mode data here
	if (isProMode)
	{
		if (isAirborne && !isAcCorrelated)
		{
			callsign = tssr;
		}

		if (!isAcCorrelated)
		{
			actype = "NoFPL";
		}

		// Is a primary target

		if (isAirborne && !isAcCorrelated && IsPrimary)
		{
			flightlevel = "NoALT";
			tendency = "?";
			speed = std::to_string(rt.GetGS());
		}

		//if (isAirborne && !isAcCorrelated && IsPrimary)
		//{
		//	callsign = TagReplacingMap["systemid"];
		//}
	}

	TagReplacingMap["callsign"] = callsign;
	TagReplacingMap["actype"] = actype;
	TagReplacingMap["sctype"] = sctype;
	TagReplacingMap["sqerror"] = sqerror;
	TagReplacingMap["deprwy"] = deprwy;
	TagReplacingMap["seprwy"] = seprwy;
	TagReplacingMap["arvrwy"] = arvrwy;
	TagReplacingMap["srvrwy"] = srvrwy;
	TagReplacingMap["gate"] = gate;
	TagReplacingMap["sate"] = sate;
	TagReplacingMap["flightlevel"] = flightlevel;
	TagReplacingMap["gs"] = speed;
	TagReplacingMap["tendency"] = tendency;
	TagReplacingMap["wake"] = wake;
	TagReplacingMap["ssr"] = tssr;
	TagReplacingMap["asid"] = dep;
	TagReplacingMap["ssid"] = ssid;
	TagReplacingMap["origin"] = origin;
	TagReplacingMap["dest"] = dest;
	TagReplacingMap["groundstatus"] = gstat;
	TagReplacingMap["scratch_pad"] = scratch_pad;

	return TagReplacingMap;
}

void CSMRRadar::OnFlightPlanDisconnect(CFlightPlan FlightPlan)
{
	Logger::info(string(__FUNCSIG__));
	string callsign = string(FlightPlan.GetCallsign());

	for (multimap<string, string>::iterator itr = DistanceTools.begin(); itr != DistanceTools.end(); ++itr)
	{
		if (itr->first == callsign || itr->second == callsign)
			itr = DistanceTools.erase(itr);
	}
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (uMsg == WM_SETCURSOR)
	{
		SetCursor(smrCursor);
		return true;
	}
	return CallWindowProc(gSourceProc, hwnd, uMsg, wParam, lParam);
}

void CSMRRadar::OnRefresh(HDC hDC, int Phase)
{
	Logger::info(string(__FUNCSIG__));

	const bool alt_mode = GetAsyncKeyState(alt_mode_keycode) & 0x8000;

	// First, we define some constants
	// These could probably be persisted across redraws,
	// but this is already much better than recalculating per target

	const Value& LabelsSettings = CurrentConfig->getActiveProfile()["labels"];
	const SolidBrush SquawkErrorColor(ColorManager->get_corrected_color("label",
	                                                                    CurrentConfig->getConfigColor(
		                                                                    LabelsSettings["squawk_error_color"])));
	const SolidBrush RimcasTextColor(
		CurrentConfig->getConfigColor(CurrentConfig->getActiveProfile()["rimcas"]["alert_text_color"]));
	const SolidBrush GroundPushColor(ColorManager->get_corrected_color("label",
	                                                                   CurrentConfig->getConfigColor(
		                                                                   LabelsSettings["groundstatus_colors"][
			                                                                   "push"])));
	const SolidBrush GroundTaxiColor(ColorManager->get_corrected_color("label",
	                                                                   CurrentConfig->getConfigColor(
		                                                                   LabelsSettings["groundstatus_colors"][
			                                                                   "taxi"])));
	const SolidBrush GroundDepaColor(ColorManager->get_corrected_color("label",
	                                                                   CurrentConfig->getConfigColor(
		                                                                   LabelsSettings["groundstatus_colors"][
			                                                                   "depa"])));
	const SolidBrush AselTextColor(ColorManager->get_corrected_color("label", Color::Yellow));

	// Changing the mouse cursor
	if (initCursor)
	{
		if (customCursor)
		{
			smrCursor = CopyCursor(
				(HCURSOR)::LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDC_SMRCURSOR), IMAGE_CURSOR, 0, 0,
					LR_SHARED));
			// This got broken because of threading as far as I can tell
			// The cursor does change for some milliseconds but gets reset almost instantly by external MFC code
		}
		else
		{
			smrCursor = (HCURSOR)::LoadCursor(nullptr, IDC_ARROW);
		}

		if (smrCursor != nullptr)
		{
			pluginWindow = GetActiveWindow();
			gSourceProc = reinterpret_cast<WNDPROC>(SetWindowLong(pluginWindow, GWL_WNDPROC,
			                                                      reinterpret_cast<LONG>(WindowProc)));
		}
		initCursor = false;
	}

	if (Phase == REFRESH_PHASE_AFTER_LISTS)
	{
		Logger::info("Phase == REFRESH_PHASE_AFTER_LISTS");
		if (!ColorSettingsDay)
		{
			// Creating the gdi+ graphics
			Graphics graphics(hDC);
			graphics.SetPageUnit(Gdiplus::UnitPixel);

			graphics.SetSmoothingMode(SmoothingModeAntiAlias);

			SolidBrush AlphaBrush(Color(CurrentConfig->getActiveProfile()["filters"]["night_alpha_setting"].GetInt(), 0,
			                            0, 0));

			CRect RadarArea(GetRadarArea());
			RadarArea.top = RadarArea.top - 1;
			RadarArea.bottom = GetChatArea().bottom;

			graphics.FillRectangle(&AlphaBrush, CopyRect(CRect(RadarArea)));

			graphics.ReleaseHDC(hDC);
		}

		this->draw_context_menu(hDC);
		Logger::info("break Phase == REFRESH_PHASE_AFTER_LISTS");
		return;
	}

	if (Phase != REFRESH_PHASE_BEFORE_TAGS)
		return;

	Logger::info("Phase != REFRESH_PHASE_BEFORE_TAGS");

	// Timer each seconds
	clock_final = clock() - clock_init;
	double delta_t = (double)clock_final / ((double)CLOCKS_PER_SEC);
	if (delta_t >= 1)
	{
		clock_init = clock();
		BLINK = !BLINK;
		RefreshAirportActivity();
	}

	if (!QDMenabled && !QDMSelectEnabled)
	{
		POINT p;
		if (GetCursorPos(&p))
		{
			if (ScreenToClient(GetActiveWindow(), &p))
			{
				mouseLocation = p;
			}
		}
	}

	Logger::info("Graphics set up");
	CDC dc;
	dc.Attach(hDC);

	// Creating the gdi+ graphics
	Graphics graphics(hDC);
	graphics.SetPageUnit(Gdiplus::UnitPixel);

	graphics.SetSmoothingMode(SmoothingModeAntiAlias);

	RECT RadarArea = GetRadarArea();
	RECT ChatArea = GetChatArea();
	RadarArea.bottom = ChatArea.top;

	AirportPositions.clear();


	CSectorElement apt;
	for (apt = GetPlugIn()->SectorFileElementSelectFirst(SECTOR_ELEMENT_AIRPORT);
	     apt.IsValid();
	     apt = GetPlugIn()->SectorFileElementSelectNext(apt, SECTOR_ELEMENT_AIRPORT))
	{
		CPosition Pos;
		apt.GetPosition(&Pos, 0);
		AirportPositions[string(apt.GetName())] = Pos;
	}

	RimcasInstance->RunwayAreas.clear();

	if (QDMSelectEnabled || QDMenabled || context_menu_for.has_value())
	{
		CRect R(GetRadarArea());
		R.top += 20;
		R.bottom = GetChatArea().top;

		R.NormalizeRect();
		AddScreenObject(DRAWING_BACKGROUND_CLICK, "", R, false, "");
	}

	Logger::info("Runway loop");
	CSectorElement rwy;
	for (rwy = GetPlugIn()->SectorFileElementSelectFirst(SECTOR_ELEMENT_RUNWAY);
	     rwy.IsValid();
	     rwy = GetPlugIn()->SectorFileElementSelectNext(rwy, SECTOR_ELEMENT_RUNWAY))
	{
		if (!startsWith(getActiveAirport().c_str(), rwy.GetAirportName()))
		{
			continue;
		}

		CPosition Left;
		rwy.GetPosition(&Left, 1);
		CPosition Right;
		rwy.GetPosition(&Right, 0);

		string runway_name = rwy.GetRunwayName(0);
		string runway_name2 = rwy.GetRunwayName(1);

		const Value& CustomMap = CurrentConfig->getAirportMapIfAny(getActiveAirport());

		RimcasInstance->AddRunwayArea(this, runway_name, runway_name2, RimcasInstance->GetRunwayArea(Left, Right));

		string RwName = runway_name + " / " + runway_name2;

		if (RimcasInstance->ClosedRunway.find(RwName) != RimcasInstance->ClosedRunway.end())
		{
			if (RimcasInstance->ClosedRunway[RwName])
			{
				if (CurrentConfig->isCustomRunwayAvail(getActiveAirport(), runway_name, runway_name2))
				{
					const Value& Runways = CustomMap["runways"];

					if (Runways.IsArray())
					{
						for (SizeType i = 0; i < Runways.Size(); i++)
						{
							if (!startsWith(runway_name.c_str(), Runways[i]["runway_name"].GetString()) && !startsWith(
								runway_name2.c_str(), Runways[i]["runway_name"].GetString()))
								continue;

							string path_name = "path";

							if (isLVP)
								path_name = "path_lvp";

							const Value& Path = Runways[i][path_name.c_str()];

							PointF lpPoints[5000];

							int k = 1;
							int l = 0;
							for (SizeType w = 0; w < Path.Size(); w++)
							{
								CPosition position;
								position.LoadFromStrings(Path[w][static_cast<SizeType>(1)].GetString(),
								                         Path[w][static_cast<SizeType>(0)].GetString());

								POINT cv = ConvertCoordFromPositionToPixel(position);
								lpPoints[l] = {REAL(cv.x), REAL(cv.y)};

								k++;
								l++;
							}

							graphics.FillPolygon(&SolidBrush(Color(150, 0, 0)), lpPoints, k - 1);

							break;
						}
					}
				}
				else
				{
					vector<CPosition> Area = RimcasInstance->GetRunwayArea(Left, Right);

					PointF lpPoints[5000];
					int w = 0;
					for (auto& Point : Area)
					{
						POINT toDraw = ConvertCoordFromPositionToPixel(Point);

						lpPoints[w] = {REAL(toDraw.x), REAL(toDraw.y)};
						w++;
					}

					graphics.FillPolygon(&SolidBrush(Color(150, 0, 0)), lpPoints, w);
				}
			}
		}
	}

	RimcasInstance->OnRefreshBegin(isLVP);

	// Gate Targets, before symbols and RPS
	this->gate_target->OnRefresh(this, &graphics, customFonts[currentFontSize]);

#pragma region symbols
	// Drawing the symbols
	Logger::info("Symbols loop");

	// First do some math, once
	// Should match rougly 30m in target diameter
	constexpr float symbol_size_meters = 30.0;
	const POINT center_screen = POINT{
		(RadarArea.right - RadarArea.left) / 2, (RadarArea.bottom - RadarArea.top) / 2
	};
	CPosition test_start_position = ConvertCoordFromPixelToPosition(center_screen);
	POINT test_end_pixels = ConvertCoordFromPositionToPixel(
		BetterHarversine(test_start_position, 0.0, symbol_size_meters));
	const unsigned char size = static_cast<unsigned char>(
		sqrtf(powf(test_end_pixels.x - center_screen.x, 2) + powf(test_end_pixels.y - center_screen.y, 2))
	);
	const unsigned char half_size = size / 2;
	// Then go be clever
	EuroScopePlugIn::CRadarTarget rt;
	for (rt = GetPlugIn()->RadarTargetSelectFirst();
	     rt.IsValid();
	     rt = GetPlugIn()->RadarTargetSelectNext(rt))
	{
		if (!rt.IsValid() || !rt.GetPosition().IsValid())
			continue;

		int reportedGs = rt.GetPosition().GetReportedGS();
		bool isAcDisplayed = isVisible(rt);

		if (!isAcDisplayed)
			continue;

		RimcasInstance->OnRefresh(rt, this, IsCorrelated(GetPlugIn()->FlightPlanSelect(rt.GetCallsign()), rt));

		CRadarTargetPositionData RtPos = rt.GetPosition();

		POINT acPosPix = ConvertCoordFromPositionToPixel(RtPos.GetPosition());

		draw_after_glow(rt, graphics);


		if (CurrentConfig->getActiveProfile()["targets"]["show_primary_target"].GetBool())
		{
			SolidBrush H_Brush(ColorManager->get_corrected_color("afterglow",
			                                                     CConfig::getConfigColor(
				                                                     CurrentConfig->getActiveProfile()["targets"][
					                                                     "target_color"])));

			const CFlightPlan fp = GetPlugIn()->FlightPlanSelect(rt.GetCallsign());
			const auto shape = PlaneShapeBuilder::build(rt.GetPosition(), fp);
			PointF lpPoints[PlaneShapeBuilder::patatoide_size];
			for (auto i = 0; i < shape.size(); ++i)
			{
				lpPoints[i] = {
					REAL(ConvertCoordFromPositionToPixel(shape[i]).x), REAL(ConvertCoordFromPositionToPixel(shape[i]).y)
				};
			}

			graphics.FillPolygon(&H_Brush, lpPoints, shape.size());
		}
		acPosPix = ConvertCoordFromPositionToPixel(RtPos.GetPosition());


		constexpr float symbol_line_thickness = 2.0;
		// Draw target symbols
		const Color color = ColorManager->get_corrected_color("target", Gdiplus::Color::White);
		const Pen pen(color, symbol_line_thickness);
		if (mouseWithin(
			{acPosPix.x - half_size, acPosPix.y - half_size, acPosPix.x + half_size, acPosPix.y + half_size}))
		{
			graphics.DrawLine(&pen, acPosPix.x, acPosPix.y - 8, acPosPix.x - 6, acPosPix.y - 12);
			graphics.DrawLine(&pen, acPosPix.x, acPosPix.y - 8, acPosPix.x + 6, acPosPix.y - 12);

			graphics.DrawLine(&pen, acPosPix.x, acPosPix.y + 8, acPosPix.x - 6, acPosPix.y + 12);
			graphics.DrawLine(&pen, acPosPix.x, acPosPix.y + 8, acPosPix.x + 6, acPosPix.y + 12);

			graphics.DrawLine(&pen, acPosPix.x - 8, acPosPix.y, acPosPix.x - 12, acPosPix.y - 6);
			graphics.DrawLine(&pen, acPosPix.x - 8, acPosPix.y, acPosPix.x - 12, acPosPix.y + 6);

			graphics.DrawLine(&pen, acPosPix.x + 8, acPosPix.y, acPosPix.x + 12, acPosPix.y - 6);
			graphics.DrawLine(&pen, acPosPix.x + 8, acPosPix.y, acPosPix.x + 12, acPosPix.y + 6);

			/*
			Stef, why are we getting interaction straight from the Windows APIS?
			That's BS, this method is only called like once a second?

			Well, some people like to use vSMR with TopSky.
			TopSky really likes drawing its own target symbols and handling its own interactivity.
			This for reasons unknown gets drawn _over_ the vSMR interactivity objects,
			thus our onclick handler never gets called.

			So we say f you we do it ourselves.
			 */
			const bool button_down = GetAsyncKeyState(VK_RBUTTON) & 0x8000; // MSB set: currently down
			const bool is_asel = strcmp(GetPlugIn()->RadarTargetSelectASEL().GetSystemID(), rt.GetSystemID()) == 0;
			if (button_down && !is_asel)
			{
				this->OnClickScreenObject(DRAWING_AC_SYMBOL, rt.GetCallsign(), POINT{mouseLocation.x, mouseLocation.y},
				                          RECT{
					                          acPosPix.x - half_size, acPosPix.y - half_size, acPosPix.x + half_size,
					                          acPosPix.y + half_size
				                          }, BUTTON_RIGHT);
			}
		}

		const bool AcisCorrelated = IsCorrelated(GetPlugIn()->FlightPlanSelect(rt.GetCallsign()), rt);
		const bool TargetIsAsel = strcmp(GetPlugIn()->FlightPlanSelectASEL().GetCallsign(), rt.GetCallsign()) == 0;

		AddScreenObject(DRAWING_AC_SYMBOL, rt.GetCallsign(),
		                {
			                acPosPix.x - half_size, acPosPix.y - half_size, acPosPix.x + half_size,
			                acPosPix.y + half_size
		                }, false,
		                AcisCorrelated ? GetBottomLine(rt.GetCallsign()).c_str() : rt.GetSystemID());

		if (!AcisCorrelated && reportedGs < 1 && !ReleaseInProgress && !AcquireInProgress)
			continue;

		if (RtPos.GetTransponderC())
		{
			graphics.DrawEllipse(&pen, acPosPix.x - (size / 2), acPosPix.y - (size / 2), size, size);
		}
		else // We still want the primary return square, but we simulate only getting a good lock if its moving
		{
			graphics.DrawRectangle(&pen, acPosPix.x - half_size, acPosPix.y - half_size, size, size);
		}

		if (TargetIsAsel)
		{
			const Color asel_color = ColorManager->get_corrected_color("target", Gdiplus::Color::Yellow);
			const Pen asel_pen(asel_color, symbol_line_thickness);
			const int asel_size = size + 2 * symbol_line_thickness + 2; // 2px spacing, plus compensation for thickness
			graphics.DrawEllipse(&asel_pen, acPosPix.x - (asel_size / 2), acPosPix.y - (asel_size / 2), asel_size,
			                     asel_size);
		}

		// Predicted Track Line
		// It starts 20 seconds away from the ac
		const Pen trackPen(color, 1);
		if (reportedGs > 50 && PredictedLength > 0)
		{
			double d = double(rt.GetPosition().GetReportedGS() * 0.514444) * 10;
			CPosition AwayBase = BetterHarversine(rt.GetPosition().GetPosition(), rt.GetTrackHeading(), d);

			d = double(rt.GetPosition().GetReportedGS() * 0.514444) * (PredictedLength * 60) - 10;
			CPosition PredictedEnd = BetterHarversine(AwayBase, rt.GetTrackHeading(), d);

			const POINT start = ConvertCoordFromPositionToPixel(AwayBase);
			const POINT end = ConvertCoordFromPositionToPixel(PredictedEnd);
			graphics.DrawLine(&trackPen, start.x, start.y, end.x, end.y);
		}
	}

#pragma endregion Drawing of the symbols

	TimePopupData.clear();
	AcOnRunway.clear();
	ColorAC.clear();
	tagAreas.clear();

	RimcasInstance->OnRefreshEnd(
		this, CurrentConfig->getActiveProfile()["rimcas"]["rimcas_stage_two_speed_threshold"].GetInt());

	graphics.SetSmoothingMode(SmoothingModeDefault);

#pragma region tags
	RectF mesureRect;
	graphics.MeasureString(L" ", wcslen(L" "), customFonts[currentFontSize], PointF(0, 0), &Gdiplus::StringFormat(),
	                       &mesureRect);
	int blankWidth = (int)mesureRect.GetRight();
	mesureRect = RectF(0, 0, 0, 0);
	graphics.MeasureString(L"AZERTYUIOPQSDFGHJKLMWXCVBN0", wcslen(L"AZERTYUIOPQSDFGHJKLMWXCVBN0"),
	                       customFonts[currentFontSize], PointF(0, 0), &Gdiplus::StringFormat(), &mesureRect);
	int oneLineHeight = (int)mesureRect.GetBottom();
	graphics.MeasureString(L"AZERTYUIOPQSDFGHJKLMWXCVBN0", wcslen(L"AZERTYUIOPQSDFGHJKLMWXCVBN0"),
	                       customFonts[currentFontSize + 10], PointF(0, 0), &Gdiplus::StringFormat(), &mesureRect);

	// Drawing the Tags
	auto tdc = TagDrawingContext{
		&graphics,
		oneLineHeight,
		blankWidth,
		&LabelsSettings,
		&SquawkErrorColor,
		&RimcasTextColor,
		&GroundPushColor,
		&GroundTaxiColor,
		&GroundDepaColor,
		&AselTextColor
	};


	// Releasing the hDC after the drawing
	// FIXME should not be necessary
	//graphics.ReleaseHDC(hDC);

	Logger::info("Tags loop");
	for (rt = GetPlugIn()->RadarTargetSelectFirst();
	     rt.IsValid();
	     rt = GetPlugIn()->RadarTargetSelectNext(rt))
	{
		this->draw_target(tdc, rt, alt_mode);
	}

#pragma endregion Drawing of the tags

	const int TextHeight = dc.GetTextExtent("60").cy;
	Logger::info("RIMCAS Loop");
	const SolidBrush timeBackgroundBrush(Color(150, 150, 150));
	for (std::map<string, bool>::iterator it = RimcasInstance->MonitoredRunwayArr.begin(); it != RimcasInstance->
	     MonitoredRunwayArr.end(); ++it)
	{
		if (!it->second || RimcasInstance->TimeTable[it->first].empty())
			continue;

		vector<int> TimeDefinition = RimcasInstance->CountdownDefinition;
		if (isLVP)
			TimeDefinition = RimcasInstance->CountdownDefinitionLVP;

		if (TimePopupAreas.find(it->first) == TimePopupAreas.end())
			TimePopupAreas[it->first] = {300, 300, 430, 300 + LONG(TextHeight * (TimeDefinition.size() + 1))};

		CRect CRectTime = TimePopupAreas[it->first];
		CRectTime.NormalizeRect();

		const auto res = graphics.FillRectangle(&timeBackgroundBrush, CRectTime.left, CRectTime.top, CRectTime.Width(), CRectTime.Height());

		// Drawing the runway name
		string tempS = it->first;
		dc.TextOutA(CRectTime.left + CRectTime.Width() / 2 - dc.GetTextExtent(tempS.c_str()).cx / 2, CRectTime.top,
		            tempS.c_str());

		int TopOffset = TextHeight;
		// Drawing the times
		for (auto& Time : TimeDefinition)
		{
			dc.SetTextColor(RGB(33, 33, 33));

			tempS = std::to_string(Time) + ": " + RimcasInstance->TimeTable[it->first][Time];
			if (RimcasInstance->AcColor.find(RimcasInstance->TimeTable[it->first][Time]) != RimcasInstance->AcColor.
				end())
			{
				CBrush RimcasBrush(RimcasInstance->GetAircraftColor(RimcasInstance->TimeTable[it->first][Time],
				                                                    Color::Black,
				                                                    Color::Black,
				                                                    CurrentConfig->getConfigColor(
					                                                    CurrentConfig->getActiveProfile()["rimcas"][
						                                                    "background_color_stage_one"]),
				                                                    CurrentConfig->getConfigColor(
					                                                    CurrentConfig->getActiveProfile()["rimcas"][
						                                                    "background_color_stage_two"])).ToCOLORREF()
				);

				CRect TempRect = {
					CRectTime.left, CRectTime.top + TopOffset, CRectTime.right, CRectTime.top + TopOffset + TextHeight
				};
				TempRect.NormalizeRect();

				dc.FillRect(TempRect, &RimcasBrush);
				dc.SetTextColor(RGB(238, 238, 208));
			}

			dc.TextOutA(CRectTime.left, CRectTime.top + TopOffset, tempS.c_str());

			TopOffset += TextHeight;
		}

		AddScreenObject(RIMCAS_IAW, it->first.c_str(), CRectTime, true, "");
	}

	Logger::info("Menu bar lists");

	if (ShowLists["Conflict Alert ARR"])
	{
		GetPlugIn()->OpenPopupList(ListAreas["Conflict Alert ARR"], "CA Arrival", 1);
		for (std::map<string, CRimcas::RunwayAreaType>::iterator it = RimcasInstance->RunwayAreas.begin(); it !=
		     RimcasInstance->RunwayAreas.end(); ++it)
		{
			GetPlugIn()->AddPopupListElement(it->first.c_str(), "", RIMCAS_CA_ARRIVAL_FUNC, false,
			                                 RimcasInstance->MonitoredRunwayArr[it->first.c_str()]);
		}
		GetPlugIn()->AddPopupListElement("Close", "", RIMCAS_CLOSE, false, 2, false, true);
		ShowLists["Conflict Alert ARR"] = false;
	}

	if (ShowLists["Conflict Alert DEP"])
	{
		GetPlugIn()->OpenPopupList(ListAreas["Conflict Alert DEP"], "CA Departure", 1);
		for (std::map<string, CRimcas::RunwayAreaType>::iterator it = RimcasInstance->RunwayAreas.begin(); it !=
		     RimcasInstance->RunwayAreas.end(); ++it)
		{
			GetPlugIn()->AddPopupListElement(it->first.c_str(), "", RIMCAS_CA_MONITOR_FUNC, false,
			                                 RimcasInstance->MonitoredRunwayDep[it->first.c_str()]);
		}
		GetPlugIn()->AddPopupListElement("Close", "", RIMCAS_CLOSE, false, 2, false, true);
		ShowLists["Conflict Alert DEP"] = false;
	}

	if (ShowLists["Runway closed"])
	{
		GetPlugIn()->OpenPopupList(ListAreas["Runway closed"], "Runway Closed", 1);
		for (std::map<string, CRimcas::RunwayAreaType>::iterator it = RimcasInstance->RunwayAreas.begin(); it !=
		     RimcasInstance->RunwayAreas.end(); ++it)
		{
			GetPlugIn()->AddPopupListElement(it->first.c_str(), "", RIMCAS_CLOSED_RUNWAYS_FUNC, false,
			                                 RimcasInstance->ClosedRunway[it->first.c_str()]);
		}
		GetPlugIn()->AddPopupListElement("Close", "", RIMCAS_CLOSE, false, 2, false, true);
		ShowLists["Runway closed"] = false;
	}

	if (ShowLists["Visibility"])
	{
		GetPlugIn()->OpenPopupList(ListAreas["Visibility"], "Visibility", 1);
		GetPlugIn()->AddPopupListElement("Normal", "", RIMCAS_UPDATE_LVP, false, int(!isLVP));
		GetPlugIn()->AddPopupListElement("Low", "", RIMCAS_UPDATE_LVP, false, int(isLVP));
		GetPlugIn()->AddPopupListElement("Close", "", RIMCAS_CLOSE, false, 2, false, true);
		ShowLists["Visibility"] = false;
	}

	if (ShowLists["Profiles"])
	{
		GetPlugIn()->OpenPopupList(ListAreas["Profiles"], "Profiles", 1);
		vector<string> allProfiles = CurrentConfig->getAllProfiles();
		for (std::vector<string>::iterator it = allProfiles.begin(); it != allProfiles.end(); ++it)
		{
			GetPlugIn()->AddPopupListElement(it->c_str(), "", RIMCAS_UPDATE_PROFILE, false,
			                                 int(CurrentConfig->isItActiveProfile(it->c_str())));
		}
		GetPlugIn()->AddPopupListElement("Close", "", RIMCAS_CLOSE, false, 2, false, true);
		ShowLists["Profiles"] = false;
	}

	if (ShowLists["Colour Settings"])
	{
		GetPlugIn()->OpenPopupList(ListAreas["Colour Settings"], "Colour Settings", 1);
		GetPlugIn()->AddPopupListElement("Day", "", RIMCAS_UPDATE_BRIGHNESS, false, int(ColorSettingsDay));
		GetPlugIn()->AddPopupListElement("Night", "", RIMCAS_UPDATE_BRIGHNESS, false, int(!ColorSettingsDay));
		GetPlugIn()->AddPopupListElement("Close", "", RIMCAS_CLOSE, false, 2, false, true);
		ShowLists["Colour Settings"] = false;
	}

	if (ShowLists["Label Font Size"])
	{
		GetPlugIn()->OpenPopupList(ListAreas["Label Font Size"], "Label Font Size", 1);
		GetPlugIn()->AddPopupListElement("Size 1", "", RIMCAS_UPDATE_FONTS, false, int(bool(currentFontSize == 1)));
		GetPlugIn()->AddPopupListElement("Size 2", "", RIMCAS_UPDATE_FONTS, false, int(bool(currentFontSize == 2)));
		GetPlugIn()->AddPopupListElement("Size 3", "", RIMCAS_UPDATE_FONTS, false, int(bool(currentFontSize == 3)));
		GetPlugIn()->AddPopupListElement("Size 4", "", RIMCAS_UPDATE_FONTS, false, int(bool(currentFontSize == 4)));
		GetPlugIn()->AddPopupListElement("Size 5", "", RIMCAS_UPDATE_FONTS, false, int(bool(currentFontSize == 5)));
		GetPlugIn()->AddPopupListElement("Close", "", RIMCAS_CLOSE, false, 2, false, true);
		ShowLists["Label Font Size"] = false;
	}

	if (ShowLists["GRND Trail Dots"])
	{
		GetPlugIn()->OpenPopupList(ListAreas["GRND Trail Dots"], "GRND Trail Dots", 1);
		GetPlugIn()->AddPopupListElement("0", "", RIMCAS_UPDATE_GND_TRAIL, false, int(bool(Trail_Gnd == 0)));
		GetPlugIn()->AddPopupListElement("2", "", RIMCAS_UPDATE_GND_TRAIL, false, int(bool(Trail_Gnd == 2)));
		GetPlugIn()->AddPopupListElement("4", "", RIMCAS_UPDATE_GND_TRAIL, false, int(bool(Trail_Gnd == 4)));
		GetPlugIn()->AddPopupListElement("8", "", RIMCAS_UPDATE_GND_TRAIL, false, int(bool(Trail_Gnd == 8)));
		GetPlugIn()->AddPopupListElement("Close", "", RIMCAS_CLOSE, false, 2, false, true);
		ShowLists["GRND Trail Dots"] = false;
	}

	if (ShowLists["APPR Trail Dots"])
	{
		GetPlugIn()->OpenPopupList(ListAreas["APPR Trail Dots"], "APPR Trail Dots", 1);
		GetPlugIn()->AddPopupListElement("0", "", RIMCAS_UPDATE_APP_TRAIL, false, int(bool(Trail_App == 0)));
		GetPlugIn()->AddPopupListElement("4", "", RIMCAS_UPDATE_APP_TRAIL, false, int(bool(Trail_App == 4)));
		GetPlugIn()->AddPopupListElement("8", "", RIMCAS_UPDATE_APP_TRAIL, false, int(bool(Trail_App == 8)));
		GetPlugIn()->AddPopupListElement("12", "", RIMCAS_UPDATE_APP_TRAIL, false, int(bool(Trail_App == 12)));
		GetPlugIn()->AddPopupListElement("16", "", RIMCAS_UPDATE_APP_TRAIL, false, int(bool(Trail_App == 16)));
		GetPlugIn()->AddPopupListElement("Close", "", RIMCAS_CLOSE, false, 2, false, true);
		ShowLists["APPR Trail Dots"] = false;
	}

	if (ShowLists["Predicted Track Line"])
	{
		GetPlugIn()->OpenPopupList(ListAreas["Predicted Track Line"], "Predicted Track Line", 1);
		GetPlugIn()->AddPopupListElement("0", "", RIMCAS_UPDATE_PTL, false, int(bool(PredictedLength == 0)));
		GetPlugIn()->AddPopupListElement("1", "", RIMCAS_UPDATE_PTL, false, int(bool(PredictedLength == 1)));
		GetPlugIn()->AddPopupListElement("2", "", RIMCAS_UPDATE_PTL, false, int(bool(PredictedLength == 2)));
		GetPlugIn()->AddPopupListElement("3", "", RIMCAS_UPDATE_PTL, false, int(bool(PredictedLength == 3)));
		GetPlugIn()->AddPopupListElement("4", "", RIMCAS_UPDATE_PTL, false, int(bool(PredictedLength == 4)));
		GetPlugIn()->AddPopupListElement("5", "", RIMCAS_UPDATE_PTL, false, int(bool(PredictedLength == 5)));
		GetPlugIn()->AddPopupListElement("Close", "", RIMCAS_CLOSE, false, 2, false, true);
		ShowLists["Predicted Track Line"] = false;
	}

	if (ShowLists["Brightness"])
	{
		GetPlugIn()->OpenPopupList(ListAreas["Brightness"], "Brightness", 1);
		GetPlugIn()->AddPopupListElement("Label", "", RIMCAS_OPEN_LIST, false);
		GetPlugIn()->AddPopupListElement("Symbol", "", RIMCAS_OPEN_LIST, false);
		GetPlugIn()->AddPopupListElement("Afterglow", "", RIMCAS_OPEN_LIST, false);
		GetPlugIn()->AddPopupListElement("Close", "", RIMCAS_CLOSE, false, 2, false, true);
		ShowLists["Brightness"] = false;
	}

	if (ShowLists["Label"])
	{
		GetPlugIn()->OpenPopupList(ListAreas["Label"], "Label Brightness", 1);
		for (int i = CColorManager::bounds_low(); i <= CColorManager::bounds_high(); i += 10)
			GetPlugIn()->AddPopupListElement(std::to_string(i).c_str(), "", RIMCAS_BRIGHTNESS_LABEL, false,
			                                 int(bool(i == ColorManager->get_brightness("label"))));

		GetPlugIn()->AddPopupListElement("Close", "", RIMCAS_CLOSE, false, 2, false, true);
		ShowLists["Label"] = false;
	}

	if (ShowLists["Symbol"])
	{
		GetPlugIn()->OpenPopupList(ListAreas["Symbol"], "Symbol Brightness", 1);
		for (int i = CColorManager::bounds_low(); i <= CColorManager::bounds_high(); i += 10)
			GetPlugIn()->AddPopupListElement(std::to_string(i).c_str(), "", RIMCAS_BRIGHTNESS_SYMBOL, false,
			                                 int(bool(i == ColorManager->get_brightness("symbol"))));

		GetPlugIn()->AddPopupListElement("Close", "", RIMCAS_CLOSE, false, 2, false, true);
		ShowLists["Symbol"] = false;
	}

	if (ShowLists["Afterglow"])
	{
		GetPlugIn()->OpenPopupList(ListAreas["Afterglow"], "Afterglow Brightness", 1);
		for (int i = CColorManager::bounds_low(); i <= CColorManager::bounds_high(); i += 10)
			GetPlugIn()->AddPopupListElement(std::to_string(i).c_str(), "", RIMCAS_BRIGHTNESS_AFTERGLOW, false,
			                                 int(bool(i == ColorManager->get_brightness("afterglow"))));

		GetPlugIn()->AddPopupListElement("Close", "", RIMCAS_CLOSE, false, 2, false, true);
		ShowLists["Afterglow"] = false;
	}

	if (ShowLists["Belux pro mode"])
	{
		GetPlugIn()->OpenPopupList(ListAreas["Belux pro mode"], "Belux pro mode", 1);
		GetPlugIn()->AddPopupListElement("Enable", "", BELUX_SETPROMODE, false, int(belux_promode));
		GetPlugIn()->AddPopupListElement("Easy mode", "", BELUX_SETPROMODEEASY, false, int(belux_promode_easy));
		GetPlugIn()->AddPopupListElement("Close", "", RIMCAS_CLOSE, false, 2, false, true);
		ShowLists["Belux pro mode"] = false;
	}

	Logger::info("QRD");

	//---------------------------------
	// QRD
	//---------------------------------

	if (QDMenabled || QDMSelectEnabled || (DistanceToolActive && ActiveDistance.first != ""))
	{
		CPen Pen(PS_SOLID, 1, RGB(255, 255, 255));
		CPen* oldPen = dc.SelectObject(&Pen);

		POINT AirportPos = ConvertCoordFromPositionToPixel(AirportPositions[getActiveAirport()]);
		CPosition AirportCPos = AirportPositions[getActiveAirport()];
		if (QDMSelectEnabled)
		{
			AirportPos = QDMSelectPt;
			AirportCPos = ConvertCoordFromPixelToPosition(QDMSelectPt);
		}
		if (DistanceToolActive)
		{
			CPosition r = GetPlugIn()->RadarTargetSelect(ActiveDistance.first.c_str()).GetPosition().GetPosition();
			AirportPos = ConvertCoordFromPositionToPixel(r);
			AirportCPos = r;
		}
		dc.MoveTo(AirportPos);
		POINT point = mouseLocation;
		dc.LineTo(point);

		CPosition CursorPos = ConvertCoordFromPixelToPosition(point);
		double Distance = AirportCPos.DistanceTo(CursorPos);
		double Bearing = AirportCPos.DirectionTo(CursorPos);

		Gdiplus::Pen WhitePen(Color::White);
		graphics.DrawEllipse(&WhitePen, point.x - 5, point.y - 5, 10, 10);

		Distance = Distance / 0.00053996f;

		Distance = round(Distance * 10) / 10;

		Bearing = round(Bearing * 10) / 10;

		POINT TextPos = {point.x + 20, point.y};

		if (!DistanceToolActive)
		{
			string distances = std::to_string(Distance);
			size_t decimal_pos = distances.find(".");
			distances = distances.substr(0, decimal_pos + 2);

			string bearings = std::to_string(Bearing);
			decimal_pos = bearings.find(".");
			bearings = bearings.substr(0, decimal_pos + 2);

			string text = bearings;
			text += "° / ";
			text += distances;
			text += "m";
			COLORREF old_color = dc.SetTextColor(RGB(255, 255, 255));
			dc.TextOutA(TextPos.x, TextPos.y, text.c_str());
			dc.SetTextColor(old_color);
		}

		dc.SelectObject(oldPen);
		RequestRefresh();
	}

	// Distance tools here
	for (auto&& kv : DistanceTools)
	{
		CRadarTarget one = GetPlugIn()->RadarTargetSelect(kv.first.c_str());
		CRadarTarget two = GetPlugIn()->RadarTargetSelect(kv.second.c_str());

		if (!isVisible(one) || !isVisible(two))
			continue;

		CPen Pen(PS_SOLID, 1, RGB(255, 255, 255));
		CPen* oldPen = dc.SelectObject(&Pen);

		POINT onePoint = ConvertCoordFromPositionToPixel(one.GetPosition().GetPosition());
		POINT twoPoint = ConvertCoordFromPositionToPixel(two.GetPosition().GetPosition());

		dc.MoveTo(onePoint);
		dc.LineTo(twoPoint);

		POINT TextPos = {twoPoint.x + 20, twoPoint.y};

		double Distance = one.GetPosition().GetPosition().DistanceTo(two.GetPosition().GetPosition());
		double Bearing = one.GetPosition().GetPosition().DirectionTo(two.GetPosition().GetPosition());

		string distances = std::to_string(Distance);
		size_t decimal_pos = distances.find(".");
		distances = distances.substr(0, decimal_pos + 2);

		string bearings = std::to_string(Bearing);
		decimal_pos = bearings.find(".");
		bearings = bearings.substr(0, decimal_pos + 2);

		string text = bearings;
		text += "° / ";
		text += distances;
		text += "nm";
		COLORREF old_color = dc.SetTextColor(RGB(0, 0, 0));

		CRect ClickableRect = {
			TextPos.x - 2, TextPos.y, TextPos.x + dc.GetTextExtent(text.c_str()).cx + 2,
			TextPos.y + dc.GetTextExtent(text.c_str()).cy
		};
		graphics.FillRectangle(&SolidBrush(Color(127, 122, 122)), CopyRect(ClickableRect));
		dc.Draw3dRect(ClickableRect, RGB(75, 75, 75), RGB(45, 45, 45));
		dc.TextOutA(TextPos.x, TextPos.y, text.c_str());

		AddScreenObject(RIMCAS_DISTANCE_TOOL, string(kv.first + "," + kv.second).c_str(), ClickableRect, false, "");

		dc.SetTextColor(old_color);

		dc.SelectObject(oldPen);
	}

	//---------------------------------
	// Drawing the toolbar
	//---------------------------------

	Logger::info("Menu Bar");

	COLORREF qToolBarColor = RGB(127, 122, 122);

	// Drawing the toolbar on the top
	constexpr char shift_amount = 21;
	CRect ToolBarAreaTop(RadarArea.left, RadarArea.top + (shift_top_bar ? shift_amount : 0), RadarArea.right,
	                     RadarArea.top + 20 + (shift_top_bar ? shift_amount : 0));
	dc.FillSolidRect(ToolBarAreaTop, qToolBarColor);

	COLORREF oldTextColor = dc.SetTextColor(RGB(0, 0, 0));

	int offset = 2;
	dc.TextOutA(ToolBarAreaTop.left + offset, ToolBarAreaTop.top + 4, getActiveAirport().c_str());
	AddScreenObject(RIMCAS_ACTIVE_AIRPORT, "ActiveAirport", {
		                ToolBarAreaTop.left + offset, ToolBarAreaTop.top + 4,
		                ToolBarAreaTop.left + offset + dc.GetTextExtent(getActiveAirport().c_str()).cx,
		                ToolBarAreaTop.top + 4 + dc.GetTextExtent(getActiveAirport().c_str()).cy
	                }, false, "Active Airport");

	offset += dc.GetTextExtent(getActiveAirport().c_str()).cx + 10;
	dc.TextOutA(ToolBarAreaTop.left + offset, ToolBarAreaTop.top + 4, "Display");
	AddScreenObject(RIMCAS_MENU, "DisplayMenu", {
		                ToolBarAreaTop.left + offset, ToolBarAreaTop.top + 4,
		                ToolBarAreaTop.left + offset + dc.GetTextExtent("Display").cx,
		                ToolBarAreaTop.top + 4 + dc.GetTextExtent("Display").cy
	                }, false, "Display menu");

	offset += dc.GetTextExtent("Display").cx + 10;
	dc.TextOutA(ToolBarAreaTop.left + offset, ToolBarAreaTop.top + 4, "Target");
	AddScreenObject(RIMCAS_MENU, "TargetMenu", {
		                ToolBarAreaTop.left + offset, ToolBarAreaTop.top + 4,
		                ToolBarAreaTop.left + offset + dc.GetTextExtent("Target").cx,
		                ToolBarAreaTop.top + 4 + dc.GetTextExtent("Target").cy
	                }, false, "Target menu");

	offset += dc.GetTextExtent("Target").cx + 10;
	dc.TextOutA(ToolBarAreaTop.left + offset, ToolBarAreaTop.top + 4, "Colours");
	AddScreenObject(RIMCAS_MENU, "ColourMenu", {
		                ToolBarAreaTop.left + offset, ToolBarAreaTop.top + 4,
		                ToolBarAreaTop.left + offset + dc.GetTextExtent("Colour").cx,
		                ToolBarAreaTop.top + 4 + dc.GetTextExtent("Colour").cy
	                }, false, "Colour menu");

	offset += dc.GetTextExtent("Colours").cx + 10;
	dc.TextOutA(ToolBarAreaTop.left + offset, ToolBarAreaTop.top + 4, "Alerts");
	AddScreenObject(RIMCAS_MENU, "RIMCASMenu", {
		                ToolBarAreaTop.left + offset, ToolBarAreaTop.top + 4,
		                ToolBarAreaTop.left + offset + dc.GetTextExtent("Alerts").cx,
		                ToolBarAreaTop.top + 4 + dc.GetTextExtent("Alerts").cy
	                }, false, "RIMCAS menu");

	offset += dc.GetTextExtent("Alerts").cx + 10;
	dc.TextOutA(ToolBarAreaTop.left + offset, ToolBarAreaTop.top + 4, "Filters");
	AddScreenObject(RIMCAS_MENU, "FilterMenu", {
		                ToolBarAreaTop.left + offset, ToolBarAreaTop.top + 4,
		                ToolBarAreaTop.left + offset + dc.GetTextExtent("Filters").cx,
		                ToolBarAreaTop.top + 4 + dc.GetTextExtent("Filters").cy
	                }, false, "Filter menu");

	offset += dc.GetTextExtent("Filters").cx + 10;
	dc.TextOutA(ToolBarAreaTop.left + offset, ToolBarAreaTop.top + 4, "Belux");
	AddScreenObject(RIMCAS_MENU, "BeluxMenu", {
		                ToolBarAreaTop.left + offset, ToolBarAreaTop.top + 4,
		                ToolBarAreaTop.left + offset + dc.GetTextExtent("Belux").cx,
		                ToolBarAreaTop.top + 4 + +dc.GetTextExtent("Belux").cy
	                }, false, "Belux menu");


	offset += dc.GetTextExtent("Belux").cx + 10;
	dc.TextOutA(ToolBarAreaTop.left + offset, ToolBarAreaTop.top + 4, "/");
	CRect barDistanceRect = {
		ToolBarAreaTop.left + offset - 2, ToolBarAreaTop.top + 4,
		ToolBarAreaTop.left + offset + dc.GetTextExtent("/").cx, ToolBarAreaTop.top + 4 + +dc.GetTextExtent("/").cy
	};
	if (DistanceToolActive)
	{
		graphics.DrawRectangle(&Pen(Color::White), CopyRect(barDistanceRect));
	}
	AddScreenObject(RIMCAS_MENU, "/", barDistanceRect, false, "Distance tool");

	dc.SetTextColor(oldTextColor);


	//
	// App windows
	//

	Logger::info("App window rendering");

	for (std::map<int, bool>::iterator it = appWindowDisplays.begin(); it != appWindowDisplays.end(); ++it)
	{
		if (!it->second)
			continue;

		int appWindowId = it->first;
		appWindows[appWindowId]->render(hDC, this, &graphics, mouseLocation, DistanceTools);
	}

	dc.Detach();

	Logger::info("END " + string(__FUNCSIG__));
}

// ReSharper restore CppMsExtAddressOfClassRValue

//---EuroScopePlugInExitCustom-----------------------------------------------

void CSMRRadar::EuroScopePlugInExitCustom()
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState())

	if (smrCursor != nullptr && smrCursor != nullptr)
	{
		SetWindowLong(pluginWindow, GWL_WNDPROC, (LONG)gSourceProc);
	}
}

void CSMRRadar::manually_correlate(const char* system_id)
{
	AcquireInProgress = NeedCorrelateCursor = false;

	if (!is_manually_correlated(system_id))
	{
		ManuallyCorrelated.push_back(system_id);

		if (std::find(ReleasedTracks.begin(), ReleasedTracks.end(), system_id) != ReleasedTracks.end())
			ReleasedTracks.erase(std::find(ReleasedTracks.begin(), ReleasedTracks.end(), system_id));
	}
	else
	{
		ManuallyCorrelated.erase(std::find(ManuallyCorrelated.begin(), ManuallyCorrelated.end(),
		                                   system_id));
	}
}

void CSMRRadar::manually_release(const char* system_id)
{
	ReleaseInProgress = NeedCorrelateCursor = false;

	if (!is_manually_released(system_id))
	{
		ReleasedTracks.push_back(system_id);

		if (std::find(ManuallyCorrelated.begin(), ManuallyCorrelated.end(), system_id) !=
			ManuallyCorrelated.end())
			ManuallyCorrelated.erase(std::find(ManuallyCorrelated.begin(), ManuallyCorrelated.end(),
			                                   system_id));
	}
	else
	{
		ReleasedTracks.erase(std::find(ReleasedTracks.begin(), ReleasedTracks.end(), system_id));
	}
}

void CSMRRadar::draw_after_glow(CRadarTarget rt, Graphics& graphics)
{
	const auto ground_speed = rt.GetGS();

	constexpr size_t afterglow_count = 3;

	if (ground_speed <= 5) return;

	const CRadarTargetPositionData current_position = rt.GetPosition();


	// First, fill with historic positions
	CRadarTargetPositionData historic_positions[afterglow_count];
	CRadarTargetPositionData previous_position = current_position;
	for (auto i = 0; i < afterglow_count; i++)
	{
		previous_position = rt.GetPreviousPosition(previous_position);
		historic_positions[i] = previous_position;
	}

	// Reusable buffer
	PointF pixel_points[PlaneShapeBuilder::patatoide_size];
	// Render oldest afterglow first (requires signed math)
	for (int i = afterglow_count - 1; i >= 0; --i)
	{
		std::string color_name;
		switch (i)
		{
		case 0:
			color_name = "history_one_color";
			break;
		case 1:
			color_name = "history_two_color";
			break;
		default:
			color_name = "history_three_color";
			break;
		}
		const SolidBrush brush(ColorManager->get_corrected_color(
			"afterglow",
			CConfig::getConfigColor(CurrentConfig->getActiveProfile()["targets"][color_name.c_str()])
		));

		const auto pos = historic_positions[i];
		const auto fp = rt.GetCorrelatedFlightPlan();
		const auto shape = PlaneShapeBuilder::build(pos, fp, false);

		// Convert CPositions to pixel positions
		for (auto j = 0; j < shape.size(); ++j)
		{
			const auto pixel = ConvertCoordFromPositionToPixel(shape[j]);
			pixel_points[j] = PointF{
				static_cast<float>(pixel.x), static_cast<float>(pixel.y)
			};
		}
		graphics.FillPolygon(&brush, pixel_points, shape.size());
	}

	const int TrailNumber = ground_speed > 50 ? Trail_App : Trail_Gnd;
	CRadarTargetPositionData previousPos = rt.GetPreviousPosition(rt.GetPosition());
	const SolidBrush trail_brush(ColorManager->get_corrected_color("symbol", Gdiplus::Color::White));
	for (int j = 1; j <= TrailNumber; j++)
	{
		const POINT pCoord = ConvertCoordFromPositionToPixel(previousPos.GetPosition());

		graphics.FillRectangle(&trail_brush, pCoord.x - 1, pCoord.y - 1, 2, 2);

		previousPos = rt.GetPreviousPosition(previousPos);
	}
}

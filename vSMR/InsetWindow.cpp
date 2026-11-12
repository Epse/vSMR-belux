#include "stdafx.h"
#include "InsetWindow.h"
#include <utility>


CInsetWindow::CInsetWindow(int Id, CSMRRadar* radar)
{
	m_Id = Id;
	this->radar = radar;

	// Make the button font!
	LOGFONT lf;
	memset(&lf, 0, sizeof(LOGFONT));
	lf.lfHeight = topbar_height - 2 - 2; // 2 px padding around buttons
	VERIFY(button_font.CreateFontIndirectA(&lf));

	lf.lfHeight = topbar_height;
	lf.lfPitchAndFamily = FIXED_PITCH | FF_MODERN;
	VERIFY(title_font.CreateFontIndirectA(&lf));

	layers = radar->CurrentConfig->getActiveProfile().approach_insets.default_layers;
}

CInsetWindow::~CInsetWindow()
{
	button_font.DeleteObject();
}

void CInsetWindow::setAirport(const string icao)
{
	this->icao = icao;
}

void CInsetWindow::OnClickScreenObject(const char* sItemString, POINT Pt, int Button)
{
	if (Button == EuroScopePlugIn::BUTTON_RIGHT)
	{
		if (m_TagAngles.find(sItemString) != m_TagAngles.end())
		{
			m_TagAngles[sItemString] = fmod(m_TagAngles[sItemString] + 45.0, 360.0);
		}
		else
		{
			m_TagAngles[sItemString] = 45.0;
		}
	}

	if (Button == EuroScopePlugIn::BUTTON_LEFT)
	{
		if (m_TagAngles.find(sItemString) != m_TagAngles.end())
		{
			m_TagAngles[sItemString] = fmod(m_TagAngles[sItemString] - 45.0, 360.0);
		}
		else
		{
			m_TagAngles[sItemString] = 45.0;
		}
	}
}

bool CInsetWindow::OnMoveScreenObject(const char* sObjectId, POINT Pt, RECT Area, bool Released)
{
	if (strcmp(sObjectId, "window") == 0)
	{
		if (!this->m_Grip)
		{
			m_OffsetInit = m_Offset;
			m_OffsetDrag = Pt;
			m_Grip = true;
		}

		POINT maxoffset = {(m_Area.right - m_Area.left) / 2, (m_Area.bottom - (m_Area.top + 15)) / 2};
		m_Offset.x = max(-maxoffset.x, min(maxoffset.x, m_OffsetInit.x + (Pt.x - m_OffsetDrag.x)));
		m_Offset.y = max(-maxoffset.y, min(maxoffset.y, m_OffsetInit.y + (Pt.y - m_OffsetDrag.y)));

		if (Released)
		{
			m_Grip = false;
		}
	}
	if (strcmp(sObjectId, "resize") == 0)
	{
		const POINT TopLeft = {m_Area.left, m_Area.top};
		const POINT BottomRight = {Area.right, Area.bottom};

		CRect newSize(TopLeft, BottomRight);
		newSize.NormalizeRect();

		if (newSize.Height() < 100)
		{
			newSize.top = m_Area.top;
			newSize.bottom = m_Area.bottom;
		}

		if (newSize.Width() < 300)
		{
			newSize.left = m_Area.left;
			newSize.right = m_Area.right;
		}

		m_Area = newSize;

		return Released;
	}
	if (strcmp(sObjectId, "topbar") == 0)
	{
		CRect appWindowRect(m_Area);
		appWindowRect.NormalizeRect();

		const POINT TopLeft = {Area.left, Area.bottom + 1};
		const POINT BottomRight = {TopLeft.x + appWindowRect.Width(), TopLeft.y + appWindowRect.Height()};
		CRect newPos(TopLeft, BottomRight);
		newPos.NormalizeRect();

		m_Area = newPos;

		return Released;
	}

	return true;
}

void CInsetWindow::ReloadProfile()
{
	layers = radar->CurrentConfig->getActiveProfile().approach_insets.default_layers;
}

PointF CInsetWindow::projectPoint(const CPosition& pos)
{
	RectF areaRect(m_Area.left, m_Area.top, m_Area.right - m_Area.left, m_Area.bottom - m_Area.top);

	PointF refPt(areaRect.X + areaRect.Width / 2, areaRect.Y + areaRect.Height / 2);
	refPt.X += m_Offset.x;
	refPt.Y += m_Offset.y;

	PointF out;

	const double dist = AptPositions[icao].DistanceTo(pos);
	const double dir = TrueBearing(AptPositions[icao], pos);


	out.X = refPt.X + (m_Scale * dist * sin(dir) + 0.5);
	out.Y = refPt.Y - (m_Scale * dist * cos(dir) + 0.5);

	if (m_Rotation != 0)
	{
		rotate_point(out, m_Rotation, refPt);
	}
	return out;
}

void CInsetWindow::render(HDC hDC, Graphics* gdi, POINT mouseLocation,
                          multimap<string, string> distance_tools)
{
	CDC dc;
	dc.Attach(hDC);

	if (this->m_Id == -1)
		return;

	const auto old_smoothing = gdi->GetSmoothingMode();
	gdi->SetSmoothingMode(SmoothingModeAntiAlias);

	struct Utils
	{
		static string getEnumString(TagTypes type)
		{
			if (type == TagTypes::Departure)
				return "departure";
			if (type == TagTypes::Arrival)
				return "arrival";
			if (type == TagTypes::Uncorrelated)
				return "uncorrelated";
			return "airborne";
		}

		static RECT GetAreaFromText(const CDC* dc, const string& text, const POINT Pos)
		{
			return {
				Pos.x, Pos.y, Pos.x + dc->GetTextExtent(text.c_str()).cx, Pos.y + dc->GetTextExtent(text.c_str()).cy
			};
		}

		static RECT drawToolbarButton(CDC* dc, const string& letter, const CRect& TopBar, const int left,
		                              const POINT mouseLocation)
		{
			const POINT TopLeft = {TopBar.right - left, TopBar.top + 2};
			const POINT BottomRight = {TopBar.right - (left - 11), TopBar.bottom - 2};
			CRect Rect(TopLeft, BottomRight);
			Rect.NormalizeRect();
			CBrush ButtonBrush(RGB(60, 60, 60));
			dc->FillRect(Rect, &ButtonBrush);
			dc->SetTextColor(RGB(0, 0, 0));
			dc->DrawTextA(letter.c_str(), Rect, DT_CENTER | DT_SINGLELINE | DT_VCENTER);

			if (mouseWithin(mouseLocation, Rect))
				dc->Draw3dRect(Rect, RGB(45, 45, 45), RGB(75, 75, 75));
			else
				dc->Draw3dRect(Rect, RGB(75, 75, 75), RGB(45, 45, 45));

			return Rect;
		}
	};

	icao = radar->ActiveAirport;
	AptPositions = radar->AirportPositions;

	const SolidBrush background_brush(radar->CurrentConfig->getActiveProfile().approach_insets.background_color);
	CRect windowAreaCRect(m_Area);
	windowAreaCRect.NormalizeRect();
	const RectF window_area(m_Area.left, m_Area.top, m_Area.right - m_Area.left, m_Area.bottom - m_Area.top);

	Region old_clip;
	gdi->GetClip(&old_clip);
	gdi->SetClip(window_area);

	// We create the radar
	gdi->FillRectangle(&background_brush, window_area);
	radar->AddScreenObject(m_Id, "window", m_Area, true, "");

	POINT refPt = windowAreaCRect.CenterPoint();
	refPt.x += m_Offset.x;
	refPt.y += m_Offset.y;

	// Here we draw all runways for the airport
	CSectorElement rwy;
	for (rwy = radar->GetPlugIn()->SectorFileElementSelectFirst(SECTOR_ELEMENT_RUNWAY);
	     rwy.IsValid();
	     rwy = radar->GetPlugIn()->SectorFileElementSelectNext(rwy, SECTOR_ELEMENT_RUNWAY))
	{
		if (startsWith(icao.c_str(), rwy.GetAirportName()))
		{
			const Pen runway_pen(radar->CurrentConfig->getActiveProfile().approach_insets.runway_color, 1);

			CPosition EndOne, EndTwo;
			rwy.GetPosition(&EndOne, 0);
			rwy.GetPosition(&EndTwo, 1);

			const PointF Pt1 = projectPoint(EndOne);
			const PointF Pt2 = projectPoint(EndTwo);

			gdi->DrawLine(
				&runway_pen,
				Pt1, Pt2
			);

			const std::string rwy_name1 = rwy.GetRunwayName(0);
			const std::string rwy_name2 = rwy.GetRunwayName(1);
			const auto flip_one = rwy_name1.find('R') != std::string::npos;
			const auto flip_two = rwy_name2.find('R') != std::string::npos;

			draw_extended_centreline(gdi, EndOne, EndTwo, flip_one);
			draw_extended_centreline(gdi, EndTwo, EndOne, flip_two);
		}
	}

	draw_geo(gdi);

	// Fixes
	draw_fixes(gdi);

	// Aircrafts

	const Color symbol_white = radar->ColorManager->get_corrected_color("symbol", Color::White);
	const SolidBrush white_brush(symbol_white);
	const Pen symbol_white_pen(symbol_white);

	const auto string_format = Gdiplus::StringFormat();

	CRadarTarget rt;
	for (rt = radar->GetPlugIn()->RadarTargetSelectFirst();
	     rt.IsValid();
	     rt = radar->GetPlugIn()->RadarTargetSelectNext(rt))
	{
		int radarRange = radar->CurrentConfig->getActiveProfile().filters.radar_range_nm;

		if (rt.GetGS() < 60 ||
			rt.GetPosition().GetPressureAltitude() > m_Filter ||
			!rt.IsValid() ||
			!rt.GetPosition().IsValid() ||
			rt.GetPosition().GetPosition().DistanceTo(AptPositions[icao]) > radarRange)
			continue;

		CPosition RtPos2 = rt.GetPosition().GetPosition();
		CRadarTargetPositionData RtPos = rt.GetPosition();
		auto fp = radar->GetPlugIn()->FlightPlanSelect(rt.GetCallsign());
		auto reportedGs = RtPos.GetReportedGS();

		// Filtering the targets

		PointF RtPoint, hPoint;

		RtPoint = projectPoint(RtPos2);

		CRadarTargetPositionData hPos = rt.GetPreviousPosition(rt.GetPosition());
		RectF mesureRect;
		for (int i = 1; i < radar->Trail_App; i++)
		{
			if (!hPos.IsValid())
				continue;

			hPoint = projectPoint(hPos.GetPosition());

			gdi->FillRectangle(&white_brush, hPoint.X, hPoint.Y, 1.0f, 1.0f);

			hPos = rt.GetPreviousPosition(hPos);
		}

		// Predicted Track Line
		if (radar->AirborneSpeedVector > 0)
		{
			const double distance = double(rt.GetPosition().GetReportedGS() * MPS_PER_KNOT) * (radar->
				AirborneSpeedVector);
			CPosition PredictedEnd = BetterHarversine(rt.GetPosition().GetPosition(), rt.GetTrackHeading(), distance);

			gdi->DrawLine(&symbol_white_pen, projectPoint(rt.GetPosition().GetPosition()), projectPoint(PredictedEnd));
		}

		{
			const auto brush = SolidBrush(radar->airborne_target_color(&rt));

			if (is_mlat(&rt))
			{
				// Some bozo specced this as 13px irl...
				gdi->FillRectangle(&brush, RtPoint.X - 6.0f, RtPoint.Y - 6.0f, 12.0f, 12.0f);
			}
			else
			{
				// Supposed to be 17px diameter but that's stupid
				gdi->FillEllipse(&brush, RtPoint.X - 8.0f, RtPoint.Y - 8.0f, 16.0f, 16.0f);
			}

			CRect TargetArea(RtPoint.X - 4.0f, RtPoint.Y - 4.0f, RtPoint.X + 4.0f, RtPoint.Y + 4.0f);
			TargetArea.NormalizeRect();
			radar->AddScreenObject(DRAWING_AC_SYMBOL_APPWINDOW_BASE + (m_Id - APPWINDOW_BASE), rt.GetCallsign(),
			                              TargetArea, false, radar->GetBottomLine(rt.GetCallsign()).c_str());
		}

		if (mouseWithin(mouseLocation, {
			                static_cast<LONG>(RtPoint.X) - 4, static_cast<LONG>(RtPoint.Y) - 4,
			                static_cast<LONG>(RtPoint.X) + 4, static_cast<LONG>(RtPoint.Y) + 4
		                }))
		{
			gdi->DrawLine(&symbol_white_pen, RtPoint.X, RtPoint.Y - 6.0f, RtPoint.X - 4.0f, RtPoint.Y - 10.0f);
			gdi->DrawLine(&symbol_white_pen, RtPoint.X, RtPoint.Y - 6.0f, RtPoint.X + 4.0f, RtPoint.Y - 10.0f);

			gdi->DrawLine(&symbol_white_pen, RtPoint.X, RtPoint.Y + 6.0f, RtPoint.X - 4.0f, RtPoint.Y + 10.0f);
			gdi->DrawLine(&symbol_white_pen, RtPoint.X, RtPoint.Y + 6.0f, RtPoint.X + 4.0f, RtPoint.Y + 10.0f);

			gdi->DrawLine(&symbol_white_pen, RtPoint.X - 6.0f, RtPoint.Y, RtPoint.X - 10.0f, RtPoint.Y - 4.0f);
			gdi->DrawLine(&symbol_white_pen, RtPoint.X - 6.0f, RtPoint.Y, RtPoint.X - 10.0f, RtPoint.Y + 4.0f);

			gdi->DrawLine(&symbol_white_pen, RtPoint.X + 6.0f, RtPoint.Y, RtPoint.X + 10.0f, RtPoint.Y - 4.0f);
			gdi->DrawLine(&symbol_white_pen, RtPoint.X + 6.0f, RtPoint.Y, RtPoint.X + 10.0f, RtPoint.Y + 4.0f);
		}

		constexpr int length = 50;

		PointF TagCenter;
		if (!m_TagAngles.contains(rt.GetCallsign()))
		{
			m_TagAngles[rt.GetCallsign()] = 45.0; // TODO: Not the best, ah well
		}

		TagCenter.X = RtPoint.X + static_cast<float>(length * cos(DegToRad(m_TagAngles[rt.GetCallsign()])));
		TagCenter.Y = RtPoint.Y + static_cast<float>(length * sin(DegToRad(m_TagAngles[rt.GetCallsign()])));
		// Drawing the tags, what a mess

		// ----- Generating the replacing map -----
		const bool is_manually_correlated = radar->is_manually_correlated(rt.GetSystemID());
		map<string, string> TagReplacingMap = radar->GenerateTagData(rt, fp, radar->IsCorrelated(fp, rt),
		                                                             radar->GetPlugIn()->GetTransitionAltitude(),
		                                                             icao, is_manually_correlated);

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
		TagClickableMap[TagReplacingMap["sid"]] = TagClickableMap[TagReplacingMap["shid"]] = TAG_CITEM_SID;
		TagClickableMap[TagReplacingMap["origin"]] = TAG_CITEM_FPBOX;
		TagClickableMap[TagReplacingMap["dest"]] = TAG_CITEM_FPBOX;
		TagClickableMap[TagReplacingMap["systemid"]] = TAG_CITEM_MANUALCORRELATE;
		TagClickableMap[TagReplacingMap["gstatus"]] = TAG_CITEM_GROUNDSTATUS;
		TagClickableMap[TagReplacingMap["uk_stand"]] = TAG_CITEM_UKSTAND;


		//
		// ----- Now the hard part, drawing (using gdi+) -------
		//	

		TagTypes TagType = TagTypes::Departure;
		TagTypes ColorTagType = TagTypes::Departure;

		if (fp.IsValid() && strcmp(fp.GetFlightPlanData().GetDestination(), radar->getActiveAirport().c_str()) ==
			0)
		{
			TagType = TagTypes::Arrival;
			ColorTagType = TagTypes::Arrival;
		}

		if (reportedGs > 50)
		{
			TagType = TagTypes::Airborne;

			// Is "use_departure_arrival_coloring" enabled? if not, then use the airborne colors
			bool useDepArrColors = radar->CurrentConfig->getActiveProfile().labels.airborne.
			                                     use_departure_arrival_coloring;
			if (!useDepArrColors)
			{
				ColorTagType = TagTypes::Airborne;
			}
		}

		bool AcisCorrelated = radar->IsCorrelated(radar->GetPlugIn()->FlightPlanSelect(rt.GetCallsign()),
		                                                 rt);
		if (!AcisCorrelated && reportedGs >= 3)
		{
			TagType = TagTypes::Uncorrelated;
			ColorTagType = TagTypes::Uncorrelated;
		}

		// First we need to figure out the tag size

		float TagWidth = 0, TagHeight = 0;

		mesureRect = RectF(0, 0, 0, 0);
		gdi->MeasureString(L"AZERTYUIOPQSDFGHJKLMWXCVBN", wcslen(L"AZERTYUIOPQSDFGHJKLMWXCVBN"),
		                   radar->customFonts[radar->currentFontSize], PointF(0, 0), &string_format,
		                   &mesureRect);
		const float oneLineHeight = mesureRect.GetBottom();

		const LabelConfig& LabelsSettings = radar->CurrentConfig->getActiveProfile().labels;
		const auto& LabelLines = LabelsSettings.lines(TagType);
		vector<vector<string>> ReplacedLabelLines;

		for (const auto& line : LabelLines)
		{
			vector<string> lineStringArray;

			bool empty_line = true;

			float TempTagWidth = 0;

			for (const auto& original : line)
			{
				mesureRect = RectF(0, 0, 0, 0);

				std::string element = TagReplacingMap.contains(original) ? TagReplacingMap[original] : original;
				if (element.empty()) continue;

				empty_line = false;
				lineStringArray.push_back(element);

				wstring wstr = wstring(element.begin(), element.end());
				gdi->MeasureString(wstr.c_str(), wcslen(wstr.c_str()),
				                   radar->customFonts[radar->currentFontSize], PointF(0, 0),
				                   &string_format, &mesureRect);

				TempTagWidth += mesureRect.GetRight();
			}

			if (!empty_line)
			{
				TagHeight += oneLineHeight;
			}

			TagWidth = max(TagWidth, TempTagWidth);

			ReplacedLabelLines.push_back(lineStringArray);
		}
		TagHeight = TagHeight - 2.0f;

		// Pfiou, done with that, now we can draw the actual rectangle.

		// We need to figure out if the tag color changes according to RIMCAS alerts, or not
		bool rimcasLabelOnly = radar->CurrentConfig->getActiveProfile().rimcas.rimcas_label_only;

		Color definedBackgroundColor = LabelsSettings.background_color(ColorTagType);
		if (TagType == TagTypes::Departure)
		{
			if (!TagReplacingMap["sid"].empty())
			{
				definedBackgroundColor = radar->CurrentConfig->sid_color(
					TagReplacingMap["sid"], radar->getActiveAirport()).value_or(definedBackgroundColor);
			}
			if (!TagReplacingMap["sid"].empty())
			{
				definedBackgroundColor = radar->CurrentConfig->sid_color(
					TagReplacingMap["sid"], radar->getActiveAirport()).value_or(definedBackgroundColor);
			}

			if (fp.GetFlightPlanData().GetPlanType()[0] == 'I' && TagReplacingMap["asid"].empty() && ColorTagType ==
				TagTypes::Departure)
			{
				definedBackgroundColor = LabelsSettings.departure.nosid_color;
			}
		}

		if (TagReplacingMap["actype"] == "NoFPL" && ColorTagType == TagTypes::Departure)
		{
			definedBackgroundColor = LabelsSettings.departure.nofpl_color;
		}

		Color TagBackgroundColor = radar->RimcasInstance->GetAircraftColor(rt.GetCallsign(),
			definedBackgroundColor,
			LabelsSettings.background_color_on_runway(ColorTagType),
			radar->CurrentConfig->getActiveProfile().rimcas.background_color_stage_one,
			radar->CurrentConfig->getActiveProfile().rimcas.background_color_stage_two);

		if (rimcasLabelOnly)
			TagBackgroundColor = radar->RimcasInstance->GetAircraftColor(rt.GetCallsign(),
				definedBackgroundColor,
				LabelsSettings.background_color_on_runway(ColorTagType));

		RectF TagBackgroundRect(TagCenter.X - (TagWidth / 2), TagCenter.Y - (TagHeight / 2),
		                        TagWidth, TagHeight);

		{
			SolidBrush TagBackgroundBrush(TagBackgroundColor);
			gdi->FillRectangle(&TagBackgroundBrush, TagBackgroundRect);

			SolidBrush FontColor(radar->ColorManager->get_corrected_color("label",
				LabelsSettings.text_color(ColorTagType)));
			SolidBrush SquawkErrorColor(radar->ColorManager->get_corrected_color("label",
				LabelsSettings.squawk_error_color));
			SolidBrush RimcasTextColor(radar->CurrentConfig->getActiveProfile().rimcas.alert_text_color);

			float heightOffset = 0;
			for (auto&& line : ReplacedLabelLines)
			{
				float widthOffset = 0;
				for (auto&& element : line)
				{
					SolidBrush* color = &FontColor;
					if (!TagReplacingMap["sqerror"].empty() && strcmp(element.c_str(),
					                                                  TagReplacingMap["sqerror"].c_str()) == 0)
						color = &SquawkErrorColor;

					if (radar->RimcasInstance->getAlert(rt.GetCallsign()) != CRimcas::NoAlert)
						color = &RimcasTextColor;

					RectF mRect(0, 0, 0, 0);

					wstring welement = wstring(element.begin(), element.end());

					gdi->DrawString(welement.c_str(), wcslen(welement.c_str()),
					                radar->customFonts[radar->currentFontSize],
					                PointF(TagBackgroundRect.X + widthOffset, TagBackgroundRect.Y + heightOffset),
					                &string_format, color);


					gdi->MeasureString(welement.c_str(), wcslen(welement.c_str()),
					                   radar->customFonts[radar->currentFontSize], PointF(0, 0),
					                   &string_format, &mRect);

					const CRect item_rect(
						static_cast<LONG>(TagBackgroundRect.X + widthOffset),
						static_cast<LONG>(TagBackgroundRect.Y + heightOffset),
						static_cast<LONG>(TagBackgroundRect.X + widthOffset + mRect.GetRight()),
						static_cast<LONG>(TagBackgroundRect.Y + heightOffset + mRect.GetBottom())
					);

					radar->AddScreenObject(TagClickableMap[element], rt.GetCallsign(), item_rect, false,
					                              radar->GetBottomLine(rt.GetCallsign()).c_str());

					widthOffset += mRect.GetRight();
				}

				heightOffset += oneLineHeight;
			}

			// Drawing the leader line
			const auto pen = Pen(radar->ColorManager->get_corrected_color("symbol", Color::White));
			gdi->DrawLine(&pen, RtPoint,
						  PointF(TagBackgroundRect.X + TagBackgroundRect.Width / 2, TagBackgroundRect.Y + TagBackgroundRect.Height / 2));

			// If we use a RIMCAS label only, we display it, and adapt the rectangle
			RectF oldCrectSave = TagBackgroundRect;

			if (rimcasLabelOnly)
			{
				Color RimcasLabelColor = radar->RimcasInstance->GetAircraftColor(
					rt.GetCallsign(), Color::AliceBlue, Color::AliceBlue,
					radar->CurrentConfig->getActiveProfile().rimcas.background_color_stage_one,
					radar->CurrentConfig->getActiveProfile().rimcas.background_color_stage_two);

				if (RimcasLabelColor.ToCOLORREF() != Color(Color::AliceBlue).ToCOLORREF())
				{
					int rimcas_height = 0;

					wstring wrimcas_height = wstring(L"ALERT");

					RectF RectRimcas_height;

					gdi->MeasureString(wrimcas_height.c_str(), wcslen(wrimcas_height.c_str()),
					                   radar->customFonts[radar->currentFontSize], PointF(0, 0),
					                   &string_format, &RectRimcas_height);
					rimcas_height = int(RectRimcas_height.GetBottom());

					// Drawing the rectangle

					const RectF RimcasLabelRect(TagBackgroundRect.X, TagBackgroundRect.Y - rimcas_height,
					                      TagBackgroundRect.Width, TagBackgroundRect.Height);
					const auto brush = SolidBrush(RimcasLabelColor);
					gdi->FillRectangle(&brush, RimcasLabelRect);
					TagBackgroundRect.Y -= rimcas_height;

					// Drawing the text

					const wstring rimcasw = wstring(L"ALERT");
					StringFormat stformat = new StringFormat();
					stformat.SetAlignment(StringAlignment::StringAlignmentCenter);
					gdi->DrawString(rimcasw.c_str(), rimcasw.length(),
					                radar->customFonts[radar->currentFontSize],
					                PointF( TagBackgroundRect.X + TagBackgroundRect.Width / 2.0f, TagBackgroundRect.Y), 
						&stformat, &RimcasTextColor);
				}
			}

			// Adding the tag screen object

			//radar->AddScreenObject(DRAWING_TAG, rt.GetCallsign(), TagBackgroundRect, true, GetBottomLine(rt.GetCallsign()).c_str());

			TagBackgroundRect = oldCrectSave;

			// Now adding the clickable zones
		}
	}

	// Distance tools here
	for (auto&& kv : distance_tools)
	{
		CRadarTarget one = radar->GetPlugIn()->RadarTargetSelect(kv.first.c_str());
		CRadarTarget two = radar->GetPlugIn()->RadarTargetSelect(kv.second.c_str());

		const int radarRange = radar->CurrentConfig->getActiveProfile().filters.radar_range_nm;

		if (one.GetGS() < 60 ||
			one.GetPosition().GetPressureAltitude() > m_Filter ||
			!one.IsValid() ||
			!one.GetPosition().IsValid() ||
			one.GetPosition().GetPosition().DistanceTo(AptPositions[icao]) > radarRange)
			continue;

		if (two.GetGS() < 60 ||
			two.GetPosition().GetPressureAltitude() > m_Filter ||
			!two.IsValid() ||
			!two.GetPosition().IsValid() ||
			two.GetPosition().GetPosition().DistanceTo(AptPositions[icao]) > radarRange)
			continue;

		const auto onePoint = projectPoint(one.GetPosition().GetPosition());
		const auto twoPoint = projectPoint(two.GetPosition().GetPosition());

		const Pen white_pen(Color::White);

		gdi->DrawLine(&white_pen, onePoint, twoPoint);

		const PointF TextPos = twoPoint + PointF{20.0f, 0.0f};

		double Distance = one.GetPosition().GetPosition().DistanceTo(two.GetPosition().GetPosition());
		double Bearing = one.GetPosition().GetPosition().DirectionTo(two.GetPosition().GetPosition());

		string distances = std::to_string(Distance);
		size_t decimal_pos = distances.find('.');
		distances = distances.substr(0, decimal_pos + 2);

		string bearings = std::to_string(Bearing);
		decimal_pos = bearings.find('.');
		bearings = bearings.substr(0, decimal_pos + 2);

		wstring text = wstring(bearings.begin(), bearings.end());
		text += L"° / ";
		text += wstring(distances.begin(), distances.end());
		text += L"nm";

		RectF measure_rect;
		gdi->MeasureString(text.c_str(), -1, radar->customFonts[radar->currentFontSize], TextPos, &measure_rect);

		CRect ClickableRect = {
			static_cast<LONG>(TextPos.X) - 2, 
			static_cast<LONG>(TextPos.Y), 
			static_cast<LONG>(TextPos.X + measure_rect.Width) + 2,
			static_cast<LONG>(TextPos.Y + measure_rect.Height)
		};

		{
			const auto brush = SolidBrush(Color(127, 122, 122));
			const SolidBrush text_brush(Color::Black);
			gdi->FillRectangle(&brush, CopyRect(ClickableRect));
			dc.Draw3dRect(ClickableRect, RGB(75, 75, 75), RGB(45, 45, 45));
			gdi->DrawString(text.c_str(), -1, radar->customFonts[radar->currentFontSize], TextPos, &text_brush);

			radar->AddScreenObject(RIMCAS_DISTANCE_TOOL, string(kv.first + "," + kv.second).c_str(),
			                              ClickableRect, false, "");
		}
	}

	gdi->SetClip(&old_clip);

	// Resize square
	POINT BottomRight = {m_Area.right, m_Area.bottom};
	POINT TopLeft = {BottomRight.x - 10, BottomRight.y - 10};
	CRect ResizeArea = {TopLeft, BottomRight};
	ResizeArea.NormalizeRect();
	dc.FillSolidRect(ResizeArea, RGB(60, 60, 60));
	radar->AddScreenObject(m_Id, "resize", ResizeArea, true, "");

	dc.Draw3dRect(ResizeArea, RGB(0, 0, 0), RGB(0, 0, 0));

	// Sides
	CBrush FrameBrush(RGB(33, 39, 204));
	COLORREF TopBarTextColor(RGB(255, 255, 255));
	dc.FrameRect(windowAreaCRect, &FrameBrush);

	// Topbar
	TopLeft = windowAreaCRect.TopLeft();
	TopLeft.y = TopLeft.y - topbar_height;
	BottomRight = {windowAreaCRect.right, windowAreaCRect.top};
	CRect TopBar(TopLeft, BottomRight);
	TopBar.NormalizeRect();
	dc.FillRect(TopBar, &FrameBrush);
	COLORREF oldTextColorC = dc.SetTextColor(TopBarTextColor);

	radar->AddScreenObject(m_Id, "topbar", TopBar, true, "");

	const auto old_font = dc.SelectObject(&title_font);
	const string Toptext = "INSET ";
	CRect titleRect(TopBar);
	titleRect.OffsetRect(2, 0);
	dc.DrawText(Toptext.c_str(), titleRect, DT_LEFT | DT_SINGLELINE | DT_VCENTER);

	// Select non-clipping font
	dc.SelectObject(&button_font);
	// Range button
	CRect RangeRect = Utils::drawToolbarButton(&dc, "Z", TopBar, 29, mouseLocation);
	radar->AddScreenObject(m_Id, "range", RangeRect, false, "");

	// Filter button
	CRect FilterRect = Utils::drawToolbarButton(&dc, "F", TopBar, 42, mouseLocation);
	radar->AddScreenObject(m_Id, "filters", FilterRect, false, "");

	// Rotate button
	CRect RotateRect = Utils::drawToolbarButton(&dc, "R", TopBar, 55, mouseLocation);
	radar->AddScreenObject(m_Id, "rotate", RotateRect, false, "");

	// Layer button
	CRect LayerRect = Utils::drawToolbarButton(&dc, "L", TopBar, 68, mouseLocation);
	radar->AddScreenObject(m_Id, "layer", LayerRect, false, "");

	dc.SetTextColor(oldTextColorC);

	// Close
	POINT TopLeftClose = {TopBar.right - 16, TopBar.top + 2};
	POINT BottomRightClose = {TopBar.right - 5, TopBar.bottom - 2};
	CRect CloseRect(TopLeftClose, BottomRightClose);
	CloseRect.NormalizeRect();
	CBrush CloseBrush(RGB(60, 60, 60));
	dc.FillRect(CloseRect, &CloseBrush);
	CPen BlackPen(PS_SOLID, 1, RGB(0, 0, 0));
	dc.SelectObject(BlackPen);
	dc.MoveTo(CloseRect.TopLeft());
	dc.LineTo(CloseRect.BottomRight());
	dc.MoveTo({CloseRect.right - 1, CloseRect.top});
	dc.LineTo({CloseRect.left - 1, CloseRect.bottom});

	// Reset font
	dc.SelectObject(old_font);

	if (mouseWithin(mouseLocation, CloseRect))
		dc.Draw3dRect(CloseRect, RGB(45, 45, 45), RGB(75, 75, 75));
	else
		dc.Draw3dRect(CloseRect, RGB(75, 75, 75), RGB(45, 45, 45));

	radar->AddScreenObject(m_Id, "close", CloseRect, false, "");

	gdi->SetSmoothingMode(old_smoothing);

	dc.Detach();
}

// Requires a clip to be set
void CInsetWindow::draw_fixes(Graphics* gdi)
{
	const SolidBrush brush(geo_color);
	const auto& fixes_visual = radar->CurrentConfig->getActiveProfile().approach_insets.fixes_visual;
	const auto& fixes_instrument = radar->CurrentConfig->getActiveProfile().approach_insets.fixes_instrument;

	PointF triangle[3];

	if (fixes_visual.empty() && fixes_instrument.empty()) return;

	StringFormat format;
	format.SetAlignment(StringAlignmentCenter);

	const FontFamily family(L"Euroscope");
	const Gdiplus::Font f(&family, fix_font_size, FontStyleRegular, UnitPixel);

	for (CSectorElement se = radar->GetPlugIn()->SectorFileElementSelectFirst(SECTOR_ELEMENT_FIX);
	     se.IsValid();
	     se = radar->GetPlugIn()->SectorFileElementSelectNext(se, SECTOR_ELEMENT_FIX))
	{
		const std::string name = se.GetName();
		const wstring wname(name.begin(), name.end());
		const auto found_visual = layers.vfr_fixes && ranges::find(fixes_visual, name) != fixes_visual.end();
		const auto found_instrument = layers.ifr_fixes && ranges::find(fixes_instrument, name) != fixes_instrument.end();

		if (!found_visual && !found_instrument) continue;

		CPosition position;
		se.GetPosition(&position, 0);
		const auto point = projectPoint(position);

		if (found_visual)
		{
			triangle[0] = {point.X, point.Y - triangle_size / 2};
			triangle[1] = {point.X - triangle_size / 2, point.Y + triangle_size / 2};
			triangle[2] = {point.X + triangle_size / 2, point.Y + triangle_size / 2};

			gdi->FillPolygon(&brush, triangle, 3);
			gdi->DrawString(wname.c_str(), -1, &f, {point.X, point.Y + triangle_size / 2 + 2}, &format, &brush);
		}
		else if (found_instrument)
		{
			const RectF rect{point.X - square_size / 2, point.Y - square_size / 2, square_size, square_size};
			gdi->FillRectangle(&brush, rect);
			gdi->DrawString(wname.c_str(), -1, &f, {point.X, point.Y + square_size / 2 + 2}, &format, &brush);
		}
	}

	if (fixes_instrument.empty() || !layers.ifr_fixes)
	{
		return;
	}

	for (CSectorElement se = radar->GetPlugIn()->SectorFileElementSelectFirst(SECTOR_ELEMENT_VOR);
	     se.IsValid();
	     se = radar->GetPlugIn()->SectorFileElementSelectNext(se, SECTOR_ELEMENT_VOR))
	{
		const std::string name = se.GetName();
		const wstring wname(name.begin(), name.end());

		if (ranges::find(fixes_instrument, name) == fixes_instrument.end()) continue;

		CPosition position;
		se.GetPosition(&position, 0);
		const auto point = projectPoint(position);

		const RectF rect{point.X - square_size / 2, point.Y - square_size / 2, square_size, square_size};
		gdi->FillRectangle(&brush, rect);
		gdi->DrawString(wname.c_str(), -1, &f, {point.X, point.Y + square_size / 2 + 2}, &format, &brush);
	}
}

// Requires a clip to be set for proper functioning
void CInsetWindow::draw_extended_centreline(Graphics* gdi, const CPosition& Threshold, const CPosition& OtherEnd,
                                            const bool flip_side)
{
	const Pen extended_centreline_pen(radar->CurrentConfig->getActiveProfile().approach_insets.extended_lines_color, 1);

	double reverseHeading = RadToDeg(TrueBearing(OtherEnd, Threshold));
	double lenght = static_cast<double>(radar->CurrentConfig->getActiveProfile().approach_insets.extended_lines_length)
		* 1852.0;
	const int increment = radar->CurrentConfig->getActiveProfile().approach_insets.extended_lines_tick_spacing * 1852;

	// Drawing the extended centreline
	CPosition endExtended = BetterHarversine(Threshold, reverseHeading, lenght);
	CPosition startExtended = BetterHarversine(Threshold, reverseHeading, increment);

	PointF Pt1 = projectPoint(startExtended);
	PointF Pt2 = projectPoint(endExtended);


	gdi->DrawLine(&extended_centreline_pen, Pt1, Pt2);

	// Drawing the ticks
	const size_t count = radar->CurrentConfig->getActiveProfile().approach_insets.extended_lines_length / radar->
		CurrentConfig->getActiveProfile().approach_insets.extended_lines_tick_spacing;

	const auto tick_direction = flip_side ? -90 : 90;

	for (size_t i = 1; i <= count; ++i)
	{
		const auto distance = increment * i;

		const auto tick_length = i % 2 == 0 ? 500 : 250;

		CPosition tickPosition = BetterHarversine(Threshold, reverseHeading, distance);
		CPosition tickTop = BetterHarversine(tickPosition, fmod(reverseHeading + tick_direction, 360), tick_length);


		Pt1 = projectPoint(tickPosition);
		Pt2 = projectPoint(tickTop);

		gdi->DrawLine(&extended_centreline_pen, Pt1, Pt2);
	}
}

void CInsetWindow::draw_geo(Graphics* gdi)
{
	const Pen pen(geo_color);

	if (layers.ranges_1 || layers.ranges_2)
	{
		const auto count = layers.ranges_1 * 3 + layers.ranges_2 * 2;
		// Ring every 2 miles, centred on the airport
		const auto center = AptPositions[icao];
		const auto two_miles = BetterHarversine(center, 90.0, 2 * 1852.0);

		const auto center_px = projectPoint(center);
		const auto two_miles_px = projectPoint(two_miles);

		const float radius = sqrtf(powf(two_miles_px.X - center_px.X, 2) + powf(two_miles_px.Y - center_px.Y, 2));

		// Maybe this should be configurable?? Brussels has 5, regionals seem happy with 3
		for (int i = 1; i <= count; ++i)
		{
			gdi->DrawEllipse(
				&pen,
				center_px.X - i * radius,
				center_px.Y - i * radius,
				i * radius * 2.0f,
				i * radius * 2.0f
			);
		}
	}

	if (!layers.ctr && !layers.tma) return;
	// TODO a lot of these elements are each time 2 coords forming a line, together a polygon
	// Might be better if we somehow merge them based on matching coords and draw it once as a polygon instead of many lines
	for (CSectorElement elem = radar->GetPlugIn()->SectorFileElementSelectFirst(SECTOR_ELEMENT_LOW_ARTC);
		elem.IsValid();
		elem = radar->GetPlugIn()->SectorFileElementSelectNext(elem, SECTOR_ELEMENT_LOW_ARTC))
	{
		const std::string name = elem.GetName();
		const bool draw_ctr = (layers.ctr && name.find("CTR") != std::string::npos);
		const bool draw_tma = (layers.tma && name.find("TMA") != std::string::npos);

		if (!draw_ctr && !draw_tma) continue;

		std::vector<PointF> pts;
		CPosition pos;
		for (int i = 0; elem.GetPosition(&pos, i); ++i)
		{
			pts.emplace_back(projectPoint(pos));
		}

		if (draw_ctr)
		{
			gdi->DrawPolygon(&pen, pts.data(), pts.size());
		}
		else if (draw_tma)
		{
			gdi->DrawPolygon(&pen, pts.data(), pts.size());
		}
	}
}

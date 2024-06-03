#include "stdafx.h"
#include "GateTarget.hpp"
#include <math.h>

#include "SMRRadar.hpp"
#include "HttpHelper.hpp"
#include "rapidjson/document.h"

std::optional<EuroScopePlugIn::CPosition> GateTarget::gateLocation(const Gate& gate_info)
{
	if (gate_info.gate.empty())
		return {}; // Auto-generated non-existent value

	auto pos = EuroScopePlugIn::CPosition();
	pos.m_Latitude = gate_info.latitude;
	pos.m_Longitude = gate_info.longitude;

	return pos;
}

/**
 * \brief Fetches gate info from the internet and parses it
 *
 * Call this from _beginthread
 */
void GateTarget::loadGates()
{
	if (!gates.empty())
		return;

	const std::string url = "https://api.beluxvacc.org/belux-gate-manager-api/gates";

	// Use HttpHelper to get https://api.beluxvacc.org/belux-gate-manager-api/gates
	// Then use rapidjson to parse
	// map that into gates
	/*
	const auto httpHelper = new HttpHelper();
	std::string gate_json;
	gate_json.assign(httpHelper->downloadStringFromURL(url));
	*/

	char DllPathFile[_MAX_PATH];
	GetModuleFileNameA(HINSTANCE(&__ImageBase), DllPathFile, sizeof(DllPathFile));
	std::string DllPath = DllPathFile;
	DllPath.resize(DllPath.size() - strlen("vSMR.dll"));
	std::string gatePath = DllPath + "\\gates.json";

	stringstream ss;
	ifstream ifs;
	ifs.open(gatePath, std::ios::binary);
	ss << ifs.rdbuf();
	ifs.close();

	Document doc;
	doc.Parse<0>(ss.str().c_str());

	if (doc.HasParseError())
	{
		// IDK??
		return;
	}

	for (SizeType i = 0; i < doc.Size(); i++)
	{
		const Value& val = doc[i];
		const std::string airport = val["airport"].GetString();
		const std::string gate_name = val["gate"].GetString();

		// Separately extract as not all gates have one
		const int heading = val.HasMember("heading") ? val["heading"].GetInt() : 180;

		const Gate gate{
			airport, gate_name, val["apron"].GetString(), heading,
			val["latitude"].GetDouble(), val["longitude"].GetDouble()
		};

		gates[airport][gate_name] = gate;
	}
}

void GateTarget::getIndicator(Gdiplus::Point* points, POINT target, const unsigned char width)
{
	// In signed int, to avoid narrowing casts
	const int height = (width * 16) / 10; // width*1.6, but without jumps via double
	points[0] = points[5] = Gdiplus::Point{ target.x, target.y };
	points[1] = { target.x - (width / 2), target.y - (width / 2)}; // y-width/2 gives a 45 degree angle
	points[2] = { target.x - (width / 2), target.y - height};
	points[3] = { target.x + (width / 2), target.y - height};
	points[4] = { target.x + (width / 2), target.y - (width / 2)};
}

// ASRs can be rotated at an angle, we will here calculate it as ES does not provide it
double GateTarget::calculateAsrAngle(EuroScopePlugIn::CRadarScreen* radar_screen) const
{
	const auto zero = radar_screen->ConvertCoordFromPixelToPosition(POINT{ 0, 0 });
	EuroScopePlugIn::CPosition end;
	end.m_Latitude = zero.m_Latitude;
	end.m_Longitude = zero.m_Longitude + 0.05;
	const auto px_end = radar_screen->ConvertCoordFromPositionToPixel(end);

	return RadToDeg(atan(static_cast<double>(px_end.y) / static_cast<double>(px_end.x)));
}

unsigned char GateTarget::calculateTargetSize(EuroScopePlugIn::CRadarScreen* radar_screen)
{
	constexpr float symbol_size_meters = 35.0; // Target size in metres
	const auto RadarArea = radar_screen->GetRadarArea();
	const POINT center_screen = POINT{
		(RadarArea.right - RadarArea.left) / 2, (RadarArea.bottom - RadarArea.top) / 2
	};
	const CPosition test_start_position = radar_screen->ConvertCoordFromPixelToPosition(center_screen);
	const POINT test_end_pixels = radar_screen->ConvertCoordFromPositionToPixel(
		BetterHarversine(test_start_position, 0.0, symbol_size_meters));
	return static_cast<unsigned char>(
		sqrtf(powf(test_end_pixels.x - center_screen.x, 2) + powf(test_end_pixels.y - center_screen.y, 2))
	);
}

std::optional<Gate> GateTarget::maybe_get_gate(CSMRRadar* radar_screen, const EuroScopePlugIn::CFlightPlan* fp) const
{
	const std::string gate = fp->GetControllerAssignedData().GetFlightStripAnnotation(4);
	if (gate.empty())
		return {};

	const auto airport = radar_screen->getActiveAirport();
	if (strcmp(fp->GetFlightPlanData().GetDestination(), airport.c_str()) != 0)
		return {};

	const Gate gate_info = this->gates.at(airport).at(gate);
	if (gate_info.airport.empty())
		return {};

	return gate_info;
}

void GateTarget::OnRefresh(CSMRRadar* radar_screen, Gdiplus::Graphics* graphics, Gdiplus::Font* font)
{
	const EuroScopePlugIn::CFlightPlan fp = radar_screen->GetPlugIn()->FlightPlanSelectASEL();
	const auto maybe_gate = this->maybe_get_gate(radar_screen, &fp);
	if (!maybe_gate.has_value())
		return;
	const auto gate_info = maybe_gate.value();

	const auto maybe_pos = this->gateLocation(gate_info);
	if (!maybe_pos.has_value())
		return;
	const POINT pos = radar_screen->ConvertCoordFromPositionToPixel(maybe_pos.value());

	const auto bright_blue = Gdiplus::Color(255, 0, 160, 250);
	const auto brush = Gdiplus::SolidBrush(bright_blue);
	const auto pen = Gdiplus::Pen(Gdiplus::Color::Black, 1);

	const auto size_px = GateTarget::calculateTargetSize(radar_screen);

	const double angle = calculateAsrAngle(radar_screen);
	const auto indicator_angle = (gate_info.heading  + angle);
	graphics->TranslateTransform(pos.x, pos.y);
	graphics->RotateTransform(indicator_angle - 180);
	Gdiplus::Point points[GateTarget::POINTS_IN_INDICATOR];
	this->getIndicator(points, POINT{0, 0}, size_px);
	graphics->FillPolygon(&brush, points, GateTarget::POINTS_IN_INDICATOR);
	graphics->DrawPolygon(&pen, points, GateTarget::POINTS_IN_INDICATOR);
	graphics->ResetTransform();

	graphics->TranslateTransform(pos.x, pos.y);
	const auto line_pen = Gdiplus::Pen(bright_blue, 2);
	const auto clamped_angle = (static_cast<int>(indicator_angle)) % 360;
	const auto sign = clamped_angle >= 90 && clamped_angle <= 270 ? +1 : -1;
	graphics->DrawLine(&line_pen, 0, 0, 0, sign * 30);

	const wstring wgate = wstring(gate_info.gate.begin(), gate_info.gate.end());
	auto label_text_rect = Gdiplus::RectF();
	graphics->MeasureString(wgate.c_str(), wgate.length(), font, PointF(GateTarget::LABEL_PADDING, GateTarget::LABEL_PADDING + sign * 30), &label_text_rect);
	// Look, I don't know why putting 2*padding on the Y makes it too high and also not center the text, but here we are I guess
	const auto label_rect = Gdiplus::RectF(0, sign * 30, label_text_rect.Width + 2 * GateTarget::LABEL_PADDING, label_text_rect.Height + GateTarget::LABEL_PADDING);

	const auto label_bg_brush = Gdiplus::SolidBrush(Gdiplus::Color(100, 0, 70, 195)); // The alpha is a guess
	const auto label_border_pen = Gdiplus::Pen(bright_blue);
	const auto text_brush = Gdiplus::SolidBrush(Gdiplus::Color::White);
	graphics->FillRectangle(&label_bg_brush, label_rect);
	graphics->DrawRectangle(&label_border_pen, label_rect);
	graphics->DrawString(wgate.c_str(), wgate.length(), font, Gdiplus::PointF(GateTarget::LABEL_PADDING, GateTarget::LABEL_PADDING + sign * 30), &text_brush);

	graphics->ResetTransform();
}


bool GateTarget::isOnBlocks(CSMRRadar* radar_screen, EuroScopePlugIn::CRadarTarget* target) const
{
	const auto fp = target->GetCorrelatedFlightPlan();
	const auto pos = target->GetPosition().GetPosition();

	if (radar_screen->getActiveAirport() == fp.GetFlightPlanData().GetDestination())
		return false;

	if (target->GetGS() > 0)
		return false;

	const auto maybe_gate = this->maybe_get_gate(radar_screen, &fp);
	if (!maybe_gate.has_value())
		return false;

	const auto maybe_pos = gateLocation(*maybe_gate);
	if (!maybe_pos.has_value())
		return false;

	const auto dist = pos.DistanceTo(*maybe_pos) * 1852; // Convert from NM to m

	return dist <= ON_BLOCKS_TOLERANCE;
}

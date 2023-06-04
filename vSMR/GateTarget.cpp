﻿#include "stdafx.h"
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

void GateTarget::getIndicator(Gdiplus::Point* points, POINT target)
{
	points[0] = points[5] = Gdiplus::Point{ target.x, target.y };
	points[1] = { target.x - 10, target.y - 10 };
	points[2] = { target.x - 10, target.y - 10 - 20 };
	points[3] = { target.x + 10, target.y - 10 - 20 };
	points[4] = { target.x + 10, target.y - 10 };
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

void GateTarget::OnRefresh(CSMRRadar* radar_screen, Gdiplus::Graphics* graphics)
{
	const EuroScopePlugIn::CFlightPlan fp = radar_screen->GetPlugIn()->FlightPlanSelectASEL();
	const std::string airport = radar_screen->getActiveAirport();
	const std::string gate = fp.GetControllerAssignedData().GetFlightStripAnnotation(4);
	if (gate.empty())
		return;

	if (strcmp(fp.GetFlightPlanData().GetDestination(), airport.c_str()) != 0)
		return;

	Gate gate_info = this->gates[airport][gate];
	if (gate_info.airport.empty())
		return;

	const auto maybe_pos = this->gateLocation(gate_info);
	if (!maybe_pos.has_value())
		return;
	const POINT pos = radar_screen->ConvertCoordFromPositionToPixel(maybe_pos.value());

	const auto brush = Gdiplus::SolidBrush(Gdiplus::Color(150, 255, 165, 0));

	const double angle = calculateAsrAngle(radar_screen);
	graphics->TranslateTransform(pos.x, pos.y);
	graphics->RotateTransform(gate_info.heading - 180 + angle);
	Gdiplus::Point points[GateTarget::POINTS_IN_INDICATOR];
	this->getIndicator(points, POINT{0, 0});
	graphics->FillPolygon(&brush, points, GateTarget::POINTS_IN_INDICATOR);
	graphics->ResetTransform();
}

#include "stdafx.h"
#include "GateTarget.hpp"

std::optional<POINT> GateTarget::gateLocation(const std::string &gate)
{
	// Eventually will do a lookup in a downloaded file, for now:
	auto pos = EuroScopePlugIn::CPosition();
	pos.m_Latitude = 50.90128055555555;
	pos.m_Longitude = 4.476233333333334;

	return this->radar_screen->ConvertCoordFromPositionToPixel(pos);
}

GateTarget::GateTarget(CSMRRadar* radar_screen)
{
	this->radar_screen = radar_screen;
}

// TODO orientation???
void GateTarget::getIndicator(Gdiplus::Point* points, POINT target)
{
	points[0] = points[5] = Gdiplus::Point{ target.x, target.y };
	points[1] = {target.x - 10, target.y - 10};
	points[2] = { target.x - 10, target.y - 10 - 20 };
	points[3] = { target.x + 10, target.y - 10 - 20 };
	points[4] = { target.x + 10, target.y - 10 };
}

void GateTarget::OnRefresh(Gdiplus::Graphics* graphics)
{
	const EuroScopePlugIn::CFlightPlan fp = this->radar_screen->GetPlugIn()->FlightPlanSelectASEL();
	const std::string gate = fp.GetControllerAssignedData().GetFlightStripAnnotation(4);
	if (gate.empty())
		return;

	if (strcmp(fp.GetFlightPlanData().GetDestination(), this->radar_screen->getActiveAirport().c_str()) != 0)
		return;

	const auto maybe_pos = this->gateLocation(gate);
	if (!maybe_pos.has_value())
		return;
	const auto pos = maybe_pos.value;

	const auto brush = Gdiplus::SolidBrush(Gdiplus::Color::Orange);
	Gdiplus::Point points[GateTarget::POINTS_IN_INDICATOR];
	this->getIndicator(points, pos);
	graphics->FillPolygon(&brush, points, GateTarget::POINTS_IN_INDICATOR);
}

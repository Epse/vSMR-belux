#pragma once
#include <GdiPlus.h>
#include <optional>
#include <string>

#include "EuroScopePlugIn.h"

class CSMRRadar;

class GateTarget
{
private:
	static constexpr int POINTS_IN_INDICATOR = 6;
	std::optional<EuroScopePlugIn::CPosition> gateLocation(const std::string& gate);
	static void getIndicator(Gdiplus::Point* points, POINT target);
public:
	void OnRefresh(CSMRRadar* radar_screen, Gdiplus::Graphics* graphics);
};

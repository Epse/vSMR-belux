#pragma once
#include <GdiPlus.h>
#include <optional>
#include <string>

#include "EuroScopePlugIn.h"
#include "SMRRadar.hpp"

class GateTarget
{
private:
	static constexpr int POINTS_IN_INDICATOR = 6;
	CSMRRadar* radar_screen;
	optional<POINT> gateLocation(const std::string& gate);
	static void getIndicator(Gdiplus::Point* points, POINT target);
public:
	GateTarget(CSMRRadar* radar_screen);
	void OnRefresh(Gdiplus::Graphics* graphics);
};

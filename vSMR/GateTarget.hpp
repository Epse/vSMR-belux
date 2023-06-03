#pragma once
#include <GdiPlus.h>
#include <map>
#include <optional>
#include <string>

#include "EuroScopePlugIn.h"

class CSMRRadar;

struct Gate
{
	std::string airport;
	std::string gate;
	std::string apron;
	double latitude;
	double longitude;
};

class GateTarget
{
private:
	static constexpr int POINTS_IN_INDICATOR = 6;
	std::optional<EuroScopePlugIn::CPosition> gateLocation(const std::string& airport, const std::string& gate);
	static void getIndicator(Gdiplus::Point* points, POINT target);
	std::map<std::string, std::map<std::string, Gate>> gates;
public:
	void OnRefresh(CSMRRadar* radar_screen, Gdiplus::Graphics* graphics);
	void loadGates();
};

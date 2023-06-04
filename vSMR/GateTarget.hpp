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
	int heading;
	double latitude;
	double longitude;
};

class GateTarget
{
private:
	static constexpr int POINTS_IN_INDICATOR = 6;
	static std::optional<EuroScopePlugIn::CPosition> gateLocation(const Gate& gate_info);
	static void getIndicator(Gdiplus::Point* points, POINT target);
	std::map<std::string, std::map<std::string, Gate>> gates;
	double calculateAsrAngle(EuroScopePlugIn::CRadarScreen* radar_screen) const;
public:
	void OnRefresh(CSMRRadar* radar_screen, Gdiplus::Graphics* graphics);
	void loadGates();
};

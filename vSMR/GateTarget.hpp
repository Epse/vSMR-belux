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
	static constexpr char LABEL_PADDING = 2;
	static constexpr unsigned int ON_BLOCKS_TOLERANCE = 20; // How close to the gate coordinates do we consider a plane on-blocks, in m
	static std::optional<EuroScopePlugIn::CPosition> gateLocation(const Gate& gate_info);
	static void getIndicator(Gdiplus::Point* points, POINT target, const unsigned char width_px);
	std::map<std::string, std::map<std::string, Gate>> gates;
	static double calculateAsrAngle(EuroScopePlugIn::CRadarScreen* radar_screen);
	static unsigned char calculateTargetSize(EuroScopePlugIn::CRadarScreen* radar_screen);
	std::optional<Gate> maybe_get_gate(CSMRRadar* radar_screen, const EuroScopePlugIn::CFlightPlan* fp) const;
public:
	void OnRefresh(CSMRRadar* radar_screen, Gdiplus::Graphics* graphics, Gdiplus::Font* font) const;
	void loadGates();
	bool isOnBlocks(CSMRRadar* radar_screen, EuroScopePlugIn::CRadarTarget* target) const;
};

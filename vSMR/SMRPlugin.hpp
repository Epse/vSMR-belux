#pragma once
#include "EuroScopePlugIn.h"
#include "HttpHelper.hpp"
#include "CPDLCSettingsDialog.hpp"
#include "DataLinkDialog.hpp"
#include <string>
#include <algorithm>
#include "Constant.hpp"
#include "Mmsystem.h"
#include <chrono>
#include <thread>
#include "SMRRadar.hpp"
#include "Logger.h"

#define MY_PLUGIN_NAME      "vSMR"
#define MY_PLUGIN_VERSION   "v1.15.1"
#define MY_PLUGIN_DEVELOPER "Pierre Ferran, Stef Pletinck, DrFreas, Keanu73, Lionel Bischof, blt950, Goulven Guinel, Nicola Macoir, hpeter, EvenAR, Sam W, Wenjen Zhou"
#define MY_PLUGIN_COPYRIGHT "GPL v3"
#define MY_PLUGIN_VIEW_AVISO  "SMR radar display"

using namespace std;
using namespace EuroScopePlugIn;

class CSMRPlugin :
	public EuroScopePlugIn::CPlugIn
{
	// Map of (identifier, callback)
	std::map<std::string, std::pair<std::string, std::clock_t>> type_map;

	void cleanup_type_map();

public:
	CSMRPlugin();
	virtual ~CSMRPlugin();

	//---OnCompileCommand------------------------------------------

	bool OnCompileCommand(const char* sCommandLine) override;

	//---OnFunctionCall------------------------------------------

	void OnFunctionCall(int FunctionId, const char* sItemString, POINT Pt, RECT Area) override;

	//---OnGetTagItem------------------------------------------

	void OnGetTagItem(CFlightPlan FlightPlan, CRadarTarget RadarTarget, int ItemCode, int TagData, char sItemString[16],
	                  int* pColorCode, COLORREF* pRGB, double* pFontSize) override;

	//---OnFlightPlanDisconnect------------------------------------------

	void OnFlightPlanDisconnect(CFlightPlan FlightPlan) override;

	//---OnTimer------------------------------------------

	void OnTimer(int Counter) override;

	//---OnRadarScreenCreated------------------------------------------

	CRadarScreen* OnRadarScreenCreated(const char* sDisplayName, bool NeedRadarContent, bool GeoReferenced,
	                                   bool CanBeSaved, bool CanBeCreated) override;

	void OnPlaneInformationUpdate(const char* sCallsign, const char* sLivery, const char* sPlaneType) override;

	std::optional<std::string> type_for(const std::string& callsign) const;
};

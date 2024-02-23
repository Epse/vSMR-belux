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
#define MY_PLUGIN_VERSION   "v1.9.0-beta2"
#define MY_PLUGIN_DEVELOPER "Pierre Ferran, Even Rognlien, Lionel Bischof, Daniel Lange, Juha Holopainen, Keanu Czirjak, Nicola Macoir, Stef Pletinck"
#define MY_PLUGIN_COPYRIGHT "GPL v3"
#define MY_PLUGIN_VIEW_AVISO  "SMR radar display"

using namespace std;
using namespace EuroScopePlugIn;

class CSMRPlugin :
	public EuroScopePlugIn::CPlugIn
{
public:
	CSMRPlugin();
	virtual ~CSMRPlugin();

	//---OnCompileCommand------------------------------------------

	bool OnCompileCommand(const char * sCommandLine) override;

	//---OnFunctionCall------------------------------------------

	void OnFunctionCall(int FunctionId, const char * sItemString, POINT Pt, RECT Area) override;

	//---OnGetTagItem------------------------------------------

	void OnGetTagItem(CFlightPlan FlightPlan, CRadarTarget RadarTarget, int ItemCode, int TagData, char sItemString[16], int * pColorCode, COLORREF * pRGB, double * pFontSize) override;

	//---OnFlightPlanDisconnect------------------------------------------

	void OnFlightPlanDisconnect(CFlightPlan FlightPlan) override;

	//---OnTimer------------------------------------------

	void OnTimer(int Counter) override;

	//---OnRadarScreenCreated------------------------------------------

	CRadarScreen * OnRadarScreenCreated(const char * sDisplayName, bool NeedRadarContent, bool GeoReferenced, bool CanBeSaved, bool CanBeCreated) override;
};


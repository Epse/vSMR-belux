#pragma once
#include <EuroScopePlugIn.h>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <time.h>
#include <GdiPlus.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include "Constant.hpp"
#include "CallsignLookup.hpp"
#include "Config.hpp"
#include "Rimcas.hpp"
#include "InsetWindow.h"
#include <memory>
#include <asio/io_service.hpp>
#include <thread>
#include "ColorManager.h"
#include "Logger.h"
#include <filesystem>
#include <iostream>
#include "UIHelpers.hpp"
#include "TagTypes.hpp"
#include "GateTarget.hpp"
#include "TagDrawingContext.hpp"
#include "Filters.h"
#include "PlaneShapeBuilder.h"

using namespace std;
using namespace Gdiplus;
using namespace EuroScopePlugIn;
namespace fs = std::filesystem;

namespace SMRSharedData
{
	static vector<string> ReleasedTracks;
	static vector<string> ManuallyCorrelated;
};


namespace SMRPluginSharedData
{
	static asio::io_service io_service;
}

struct ContextMenuData
{
	std::string system_id;
	std::string callsign;
};

using namespace SMRSharedData;

class CSMRRadar :
	public EuroScopePlugIn::CRadarScreen
{
private:
	GateTarget * gate_target;
	void draw_target(TagDrawingContext& tdc, CRadarTarget& rt, const bool alt_mode = false);
	bool shift_top_bar = false;
	bool show_err_lines = true;
	std::optional<ContextMenuData> context_menu_for = {};
	POINT context_menu_pos = POINT{ 0, 0 };
	void draw_context_menu(HDC hdc);
	void manually_correlate(const char* system_id);
	void manually_release(const char* system_id);

	char alt_mode_keycode = VK_MENU;

	std::unique_ptr<PlaneShapeBuilder> plane_shape_builder = std::make_unique<PlaneShapeBuilder>();

	void draw_after_glow(CRadarTarget rt, Graphics& graphics);

	void fill_runway(const std::string runway_name, const std::string runway_name2, Graphics& graphics, const Value& CustomMap, CPosition&
	                 Left, CPosition& Right, const Brush& brush);
public:
	CSMRRadar();
	virtual ~CSMRRadar();

	static map<string, string> vStripsStands;

	bool BLINK = false;

	vector<string> Active_Arrivals;

	clock_t clock_init, clock_final;

	COLORREF SMR_TARGET_COLOR = RGB(255, 242, 73);
	COLORREF SMR_H1_COLOR = RGB(0, 255, 255);
	COLORREF SMR_H2_COLOR = RGB(0, 219, 219);
	COLORREF SMR_H3_COLOR = RGB(0, 183, 183);

	map<string, bool> ClosedRunway;

	char DllPathFile[MAX_PATH];
	string DllPath;
	string ConfigPath;
	CCallsignLookup * Callsigns = nullptr;
	CColorManager * ColorManager;

	map<string, bool> ShowLists;
	map<string, RECT> ListAreas;

	map<int, bool> appWindowDisplays;

	map<string, CRect> tagAreas;
	map<string, double> TagAngles;
	map<string, int> TagLeaderLineLength;

	bool QDMenabled = false;
	bool QDMSelectEnabled = false;
	POINT QDMSelectPt;
	POINT QDMmousePt;

	bool ColorSettingsDay = true;

	bool isLVP = false;

	map<string, RECT> TimePopupAreas;

	map<int, string> TimePopupData;
	multimap<string, string> AcOnRunway;
	map<string, bool> ColorAC;

	map<string, CRimcas::RunwayAreaType> RunwayAreas;

	map<string, RECT> MenuPositions;
	map<string, bool> DisplayMenu;

	map<string, clock_t> RecentlyAutoMovedTags;

	CRimcas * RimcasInstance = nullptr;
	CConfig * CurrentConfig = nullptr;

	map<int, Gdiplus::Font *> customFonts;
	int currentFontSize = 1;

	map<string, CPosition> AirportPositions;

	bool Afterglow = true;

	int Trail_Gnd = 4;
	int Trail_App = 4;
	int PredictedLength = 0;
	bool AlwaysVector = false;

	bool NeedCorrelateCursor = false;
	bool ReleaseInProgress = false;
	bool AcquireInProgress = false;

	bool belux_promode = true;
	bool belux_promode_easy = false;

	Filters filters = new_filters();

	multimap<string, string> DistanceTools;
	bool DistanceToolActive = false;
	pair<string, string> ActiveDistance;


	string ActiveAirport = "EGKK";

	inline string getActiveAirport() {
		return ActiveAirport;
	}

	inline string setActiveAirport(const string& value) {
		return ActiveAirport = value;
	}

	//---GenerateTagData--------------------------------------------

	map<string, string> GenerateTagData(CRadarTarget Rt, CFlightPlan fp, bool isAcCorrelated, bool isProMode, int TransitionAltitude, bool useSpeedForGates, string ActiveAirport);

	inline virtual bool is_manually_correlated(const char* system_id)
	{
		return std::find(ManuallyCorrelated.begin(), ManuallyCorrelated.end(), system_id) != ManuallyCorrelated.end();
	}

	inline virtual bool is_manually_released(const char* system_id)
	{
		return std::find(ReleasedTracks.begin(), ReleasedTracks.end(), system_id) != ReleasedTracks.end();
	}

	//---IsCorrelatedFuncs---------------------------------------------

	inline virtual bool IsCorrelated(CFlightPlan fp, CRadarTarget rt)
	{

		if (belux_promode) {
			if (std::find(ManuallyCorrelated.begin(), ManuallyCorrelated.end(), rt.GetSystemID()) != ManuallyCorrelated.end())
			{
				return true;
			}
			if (!rt.GetPosition().GetTransponderC())
			{
				return false;
			}
			return true;
		}

		if (CurrentConfig->getActiveProfile()["filters"]["pro_mode"]["enable"].GetBool())
		{
			if (fp.IsValid())
			{
				bool isCorr = false;
				if (strcmp(fp.GetControllerAssignedData().GetSquawk(), rt.GetPosition().GetSquawk()) == 0)
				{
					isCorr = true;
				}

				if (CurrentConfig->getActiveProfile()["filters"]["pro_mode"]["accept_pilot_squawk"].GetBool())
				{
					isCorr = true;
				}

				if (isCorr)
				{
					const Value& sqs = CurrentConfig->getActiveProfile()["filters"]["pro_mode"]["do_not_autocorrelate_squawks"];
					for (SizeType i = 0; i < sqs.Size(); i++) {
						if (strcmp(rt.GetPosition().GetSquawk(), sqs[i].GetString()) == 0)
						{
							isCorr = false;
							break;
						}
					}
				}

				if (std::find(ManuallyCorrelated.begin(), ManuallyCorrelated.end(), rt.GetSystemID()) != ManuallyCorrelated.end())
				{
					isCorr = true;
				}

				if (std::find(ReleasedTracks.begin(), ReleasedTracks.end(), rt.GetSystemID()) != ReleasedTracks.end())
				{
					isCorr = false;
				}

				return isCorr;
			}

			return false;
		} else
		{
			// If the pro mode is not used, then the AC is always correlated
			return true;
		}
	};

	//---CorrelateCursor--------------------------------------------

	virtual void CorrelateCursor();

	//---LoadCustomFont--------------------------------------------

	virtual void LoadCustomFont();

	//---LoadProfile--------------------------------------------

	virtual void LoadProfile(string profileName);

	//---OnAsrContentLoaded--------------------------------------------

	void OnAsrContentLoaded(bool Loaded) override;

	//---OnAsrContentToBeSaved------------------------------------------

	void OnAsrContentToBeSaved() override;

	//---OnRefresh------------------------------------------------------

	void OnRefresh(HDC hDC, int Phase) override;

	//---OnClickScreenObject-----------------------------------------

	void OnClickScreenObject(int ObjectType, const char * sObjectId, POINT mouseLocation, RECT Area, int Button) override;

	//---OnMoveScreenObject---------------------------------------------

	void OnMoveScreenObject(int ObjectType, const char * sObjectId, POINT Pt, RECT Area, bool Released) override;

	//---OnOverScreenObject---------------------------------------------

	void OnOverScreenObject(int ObjectType, const char * sObjectId, POINT Pt, RECT Area) override;

	//---OnCompileCommand-----------------------------------------

	bool OnCompileCommand(const char * sCommandLine) override;

	//---RefreshAirportActivity---------------------------------------------

	virtual void RefreshAirportActivity(void);

	//---OnRadarTargetPositionUpdate---------------------------------------------

	void OnRadarTargetPositionUpdate(CRadarTarget RadarTarget) override;

	//---OnFlightPlanDisconnect---------------------------------------------

	void OnFlightPlanDisconnect(CFlightPlan FlightPlan) override;

	virtual bool isVisible(CRadarTarget rt)
	{
		const CRadarTargetPositionData RtPos = rt.GetPosition();
		const int radarRange = CurrentConfig->getActiveProfile()["filters"]["radar_range_nm"].GetInt();
		const int altitudeFilter = CurrentConfig->getActiveProfile()["filters"]["hide_above_alt"].GetInt();
		const int speedFilter = CurrentConfig->getActiveProfile()["filters"]["hide_above_spd"].GetInt();
		bool isAcDisplayed = true;

		if (AirportPositions[getActiveAirport()].DistanceTo(RtPos.GetPosition()) > radarRange)
			isAcDisplayed = false;

		if (altitudeFilter != 0) {
			if (RtPos.GetPressureAltitude() > altitudeFilter)
				isAcDisplayed = false;
		}

		if (speedFilter != 0) {
			if (RtPos.GetReportedGS() > speedFilter)
				isAcDisplayed = false;
		}

		return isAcDisplayed;
	}


	//---GetBottomLine---------------------------------------------

	virtual string GetBottomLine(const char * Callsign);

	//---LineIntersect---------------------------------------------

	/*inline virtual POINT getIntersectionPoint(POINT lineA, POINT lineB, POINT lineC, POINT lineD) {

		double x1 = lineA.x;
		double y1 = lineA.y;
		double x2 = lineB.x;
		double y2 = lineB.y;

		double x3 = lineC.x;
		double y3 = lineC.y;
		double x4 = lineD.x;
		double y4 = lineD.y;

		POINT p = { 0, 0 };

		double d = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4);
		if (d != 0) {
			double xi = ((x3 - x4) * (x1 * y2 - y1 * x2) - (x1 - x2) * (x3 * y4 - y3 * x4)) / d;
			double yi = ((y3 - y4) * (x1 * y2 - y1 * x2) - (y1 - y2) * (x3 * y4 - y3 * x4)) / d;

			p = { (int)xi, (int)yi };

		}
		return p;
	}*/

	//---OnFunctionCall-------------------------------------------------

	void OnFunctionCall(int FunctionId, const char * sItemString, POINT Pt, RECT Area) override;

	//---OnAsrContentToBeClosed-----------------------------------------

	void CSMRRadar::EuroScopePlugInExitCustom();

	//  This gets called before OnAsrContentToBeSaved()
	// -> we can't delete CurrentConfig just yet otherwise we can't save the active profile
	inline void OnAsrContentToBeClosed(void) override
	{
		delete RimcasInstance;
		//delete CurrentConfig;
		delete this;
	};
};

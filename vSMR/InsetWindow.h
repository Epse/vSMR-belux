#pragma once
#include "SMRRadar.hpp"
#include "EuroScopePlugIn.h"
#include <string>
#include <map>
#include "Logger.h"

using namespace std;
using namespace EuroScopePlugIn;

struct Layers
{
	bool vfr_fixes : 1;
	bool ifr_fixes : 1;
	bool ctr : 1;
	bool tma : 1;
	bool ranges_1 : 1;
	bool ranges_2 : 1;

	constexpr static Layers init()
	{
		return {.vfr_fixes = true, .ifr_fixes = true, .ctr = true , .tma = true, .ranges_1 = true, .ranges_2 = true};
	}
};

class CInsetWindow
{
public:
	CInsetWindow(int Id, CSMRRadar* radar);
	virtual ~CInsetWindow();

	// Definition
	int m_Id = -1, m_Scale = 15, m_Filter = 5500;
	RECT m_Area = { 200, 200, 600, 500 };
	POINT m_Offset = { 0, 0 }, m_OffsetInit = { 0, 0 }, m_OffsetDrag = { 0, 0 };
	bool m_Grip = false;
	double m_Rotation = 0;

	Layers layers = Layers::init();

	map<string, double> m_TagAngles;

	virtual void render(HDC Hdc, CSMRRadar * radar_screen, Graphics* gdi, POINT mouseLocation, multimap<string, string> distance_tools);
	virtual void setAirport(string icao);
	virtual PointF projectPoint(const CPosition& pos);
	virtual void OnClickScreenObject(const char * sItemString, POINT Pt, int Button);
	virtual bool OnMoveScreenObject(const char * sObjectId, POINT Pt, RECT Area, bool released);
	
private:
	static constexpr unsigned int topbar_height = 15;
	static constexpr auto triangle_size = 13.0f;
	static constexpr auto fix_font_size = 11.0f;
	static constexpr auto square_size = 9.0f;

	string icao;
	map<string, CPosition> AptPositions;

	CSMRRadar* radar;

	CFont button_font;
	CFont title_font;
	const Gdiplus::Color geo_color = Gdiplus::Color(145, 143, 134);

	void draw_fixes(Graphics* gdi);

	void draw_extended_centreline(Graphics* gdi, const CPosition& Threshold, const CPosition& OtherEnd, const bool flip_side = false);

	void draw_geo(Graphics* gdi);
};


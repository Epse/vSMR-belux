#pragma once

#include "stdafx.h"
#include <string>
#include "EuroScopePlugIn.h"
#define _USE_MATH_DEFINES
// ReSharper disable once CppUnusedIncludeDirective
#include <math.h>
#include <vector>
#include <sstream>
#include <iomanip>

#define VSTRIPS_PORT 53487

// 1 knot == MPS_PER_KNOT m/s
#define MPS_PER_KNOT 0.5144444

using namespace std;
using namespace EuroScopePlugIn;


constexpr BYTE TAG_DIMMING = 15;

constexpr unsigned int AFTERGLOW_CLEANUP_SEC = 10;

constexpr unsigned int CLEANUP_AFTER_SEC = 120;

constexpr int AIRBORNE_MARGIN_FT = 50;


typedef struct tagPOINT2 {
	double x;
	double y;
} POINT2;

inline static bool startsWith(const char* pre, const char* str)
{
	size_t lenpre = strlen(pre), lenstr = strlen(str);
	return lenstr < lenpre ? false : strncmp(pre, str, lenpre) == 0;
};

inline static void replaceAll(string& source, const string& from, const string& to)
{
	string newString;
	newString.reserve(source.length());  // avoids a few memory allocations

	string::size_type lastPos = 0;
	string::size_type findPos;

	while (string::npos != (findPos = source.find(from, lastPos)))
	{
		newString.append(source, lastPos, findPos - lastPos);
		newString += to;
		lastPos = findPos + from.length();
	}

	// Care for the rest after last occurrence
	newString += source.substr(lastPos);

	source.swap(newString);
};

inline static Gdiplus::Rect CopyRect(CRect& rect)
{
	return { rect.left, rect.top, rect.Width(), rect.Height() };
};

inline static std::vector<std::string>& split(const std::string& s, char delim, std::vector<std::string>& elems) {
	std::stringstream ss(s);
	std::string item;
	while (std::getline(ss, item, delim)) {
		elems.push_back(item);
	}
	return elems;
};
inline static std::vector<std::string> split(const std::string& s, char delim) {
	std::vector<std::string> elems;
	split(s, delim, elems);
	return elems;
};

inline static double TrueBearing(const CPosition& pos1, const CPosition& pos2)
{
	// returns the true bearing from pos1 to pos2
	const double lat1 = pos1.m_Latitude / 180 * M_PI;
	const double lat2 = pos2.m_Latitude / 180 * M_PI;
	const double lon1 = pos1.m_Longitude / 180 * M_PI;
	const double lon2 = pos2.m_Longitude / 180 * M_PI;
	const double dir = fmod(atan2(sin(lon2 - lon1) * cos(lat2), cos(lat1) * sin(lat2) - sin(lat1) * cos(lat2) * cos(lon2 - lon1)), 2 * M_PI) * 180 / M_PI;

	return dir / 180 * M_PI;
};

inline static POINT rotate_point(POINT p, const double angle, const POINT c)
{
	double sine = sin(angle * M_PI / 180);
	double cosi = cos(angle * M_PI / 180);

	// translate point back to origin:
	p.x -= c.x;
	p.y -= c.y;

	// rotate point
	double xnew = p.x * cosi - p.y * sine;
	double ynew = p.x * sine + p.y * cosi;

	// translate point back:
	p.x = LONG(xnew + c.x);
	p.y = LONG(ynew + c.y);
	return p;
}

inline static bool RectIntersect(const RECT& RectA, const RECT& RectB)
{
	return RectA.left < RectB.right
		&& RectA.right > RectB.left
		&& RectA.bottom < RectB.top
		&& RectA.top > RectB.bottom;
}

inline static double DistancePts(const POINT p0, const POINT p1)
{
	return sqrt((p1.x - p0.x) * (p1.x - p0.x) + (p1.y - p0.y) * (p1.y - p0.y));
}

inline static int Is_Left(const POINT& p0, const POINT& p1, const POINT& point)
{
	return ((p1.x - p0.x) * (point.y - p0.y) -
		(point.x - p0.x) * (p1.y - p0.y));
};

inline static bool Is_Inside(const POINT& point, const std::vector<POINT>& points_list)
{
	// The winding number counter.
	int winding_number = 0;

	// Loop through all edges of the polygon.
	typedef std::vector<POINT>::size_type size_type;

	const size_type size = points_list.size();

	for (size_type i = 0; i < size; ++i)             // Edge from point1 to points_list[i+1]
	{
		POINT point1(points_list[i]);
		POINT point2;

		// Wrap?
		if (i == (size - 1))
		{
			point2 = points_list[0];
		}
		else
		{
			point2 = points_list[i + 1];
		}

		if (point1.y <= point.y)                                    // start y <= point.y
		{
			if (point2.y > point.y)                                 // An upward crossing
			{
				if (Is_Left(point1, point2, point) > 0)             // Point left of edge
				{
					++winding_number;                               // Have a valid up intersect
				}
			}
		}
		else
		{
			// start y > point.y (no test needed)
			if (point2.y <= point.y)                                // A downward crossing
			{
				if (Is_Left(point1, point2, point) < 0)             // Point right of edge
				{
					--winding_number;                               // Have a valid down intersect
				}
			}
		}
	}

	return (winding_number != 0);
};

// Liang-Barsky function by Daniel White @ http://www.skytopia.com/project/articles/compsci/clipping.html
// This function inputs 8 numbers, and outputs 4 new numbers (plus a boolean value to say whether the clipped line is drawn at all).
//
inline static bool LiangBarsky(const RECT& Area, const POINT fromSrc, const POINT toSrc, POINT& ClipFrom, POINT& ClipTo)         // The output values, so declare these outside.
{
	const double edgeLeft = Area.left;
	const double edgeRight = Area.right;
	const double edgeBottom = Area.top;
	const double edgeTop = Area.bottom;

	const double x0src = fromSrc.x;
	const double y0src = fromSrc.y;
	const double x1src = toSrc.x;
	const double y1src = toSrc.y;

	double t0 = 0.0;    double t1 = 1.0;
	const double xdelta = x1src - x0src;
	const double ydelta = y1src - y0src;
	double p = 0, q = 0;

	for (int edge = 0; edge < 4; edge++) {   // Traverse through left, right, bottom, top edges.
		if (edge == 0) { p = -xdelta;    q = -(edgeLeft - x0src); }
		if (edge == 1) { p = xdelta;     q = (edgeRight - x0src); }
		if (edge == 2) { p = -ydelta;    q = -(edgeBottom - y0src); }
		if (edge == 3) { p = ydelta;     q = (edgeTop - y0src); }
		double r = q / p;
		if (p == 0 && q < 0) return false;   // Don't draw line at all. (parallel line outside)

		if (p < 0) {
			if (r > t1) return false;         // Don't draw line at all.
			else if (r > t0) t0 = r;            // Line is clipped!
		}
		else if (p > 0) {
			if (r < t0) return false;      // Don't draw line at all.
			else if (r < t1) t1 = r;         // Line is clipped!
		}
	}

	ClipFrom.x = long(x0src + t0 * xdelta);
	ClipFrom.y = long(y0src + t0 * ydelta);
	ClipTo.x = long(x0src + t1 * xdelta);
	ClipTo.y = long(y0src + t1 * ydelta);

	return true;        // (clipped) line is drawn
};

static bool mouseWithin(const POINT mouseLocation, const CRect& rect) {
	return mouseLocation.x >= rect.left + 1
		&& mouseLocation.x <= rect.right - 1
		&& mouseLocation.y >= rect.top + 1
		&& mouseLocation.y <= rect.bottom - 1;
}
//---Radians-----------------------------------------

inline static double DegToRad(const double x)
{
	return x / 180.0 * M_PI;
};

inline static double RadToDeg(const double x)
{
	return x / M_PI * 180.0;
};

inline static CPosition BetterHarversine(const CPosition& init, const double angle, const double meters)
{
	CPosition newPos;

	const double d = (meters * 0.00053996) / 60 * M_PI / 180;
	const double trk = DegToRad(angle);
	const double lat0 = DegToRad(init.m_Latitude);
	const double lon0 = DegToRad(init.m_Longitude);

	const double lat = asin(sin(lat0) * cos(d) + cos(lat0) * sin(d) * cos(trk));
	const double lon = cos(lat) == 0 ? lon0 : fmod(lon0 + asin(sin(trk) * sin(d) / cos(lat)) + M_PI, 2 * M_PI) - M_PI;

	newPos.m_Latitude = RadToDeg(lat);
	newPos.m_Longitude = RadToDeg(lon);

	return newPos;
};

inline static string padWithZeros(const int padding, const int s)
{
	stringstream ss;
	ss << setfill('0') << setw(padding) << s;
	return ss.str();
};

//


//---Haversine---------------------------------------------
// Heading in deg, distance in m
constexpr double PI = (double)M_PI;

inline CPosition Haversine(CPosition origin, double heading, double distance) {

	CPosition newPos;

	const double d = (distance*0.00053996) / 60 * PI / 180;
	const double trk = DegToRad(heading);
	const double lat0 = DegToRad(origin.m_Latitude);
	const double lon0 = DegToRad(origin.m_Longitude);

	const double lat = asin(sin(lat0) * cos(d) + cos(lat0) * sin(d) * cos(trk));
	const double lon = cos(lat) == 0 ? lon0 : fmod(lon0 + asin(sin(trk) * sin(d) / cos(lat)) + PI, 2 * PI) - PI;

	newPos.m_Latitude = RadToDeg(lat);
	newPos.m_Longitude = RadToDeg(lon);

	return newPos;
}

inline float randomizeHeading(float originHead) {
	return float(fmod(originHead + float((rand() % 5) - 2), 360));
}


enum EsConst: std::uint16_t
{
	TAG_ITEM_DATALINK_STS = 444,
	TAG_FUNC_DATALINK_MENU = 544,

	TAG_FUNC_DATALINK_CONFIRM = 545,
	TAG_FUNC_DATALINK_STBY = 546,
	TAG_FUNC_DATALINK_VOICE = 547,
	TAG_FUNC_DATALINK_RESET = 548,
	TAG_FUNC_DATALINK_MESSAGE = 549,

	DRAWING_TAG = 1211,
	DRAWING_AC_SYMBOL = 1212,
	DRAWING_BACKGROUND_CLICK = 1213,
	DRAWING_AC_SYMBOL_APPWINDOW_BASE = 1251,
	DRAWING_AC_SYMBOL_APPWINDOW1 = 1252,
	DRAWING_AC_SYMBOL_APPWINDOW2 = 1253,
	
	TAG_CITEM_NO = 1910,
	TAG_CITEM_CALLSIGN = 1911,
	TAG_CITEM_FPBOX = 1912,
	TAG_CITEM_RWY = 1913,
	TAG_CITEM_GATE = 1914,
	TAG_CITEM_MANUALCORRELATE = 1915,
	TAG_CITEM_SID = 1916,
	TAG_CITEM_GROUNDSTATUS = 1917,
	TAG_CITEM_UKSTAND = 1990,
	
	FUNC_MANUAL_CALLSIGN = 2000,

	// Context menu
	CONTEXT_ACQUIRE = 3000,
	CONTEXT_RELEASE = 3001,
	CONTEXT_LAND = 3002,

	// RIMCAS Menus & shit
	RIMCAS_CLOSE = EuroScopePlugIn::TAG_ITEM_FUNCTION_NO,
	RIMCAS_ACTIVE_AIRPORT = 7999,
	RIMCAS_ACTIVE_AIRPORT_FUNC = 8008,
	RIMCAS_MENU = 8000,
	RIMCAS_QDM_TOGGLE = 8006,
	RIMCAS_QDM_SELECT_TOGGLE = 8007,
	RIMCAS_ERR_LINE_TOGGLE = 8009,
	RIMCAS_SHIFT_TOP_BAR_TOGGLE = 8010,
	RIMCAS_OPEN_LIST = 9873,
	RIMCAS_TIMER = 8015,
	RIMCAS_UPDATE_PROFILE = 8016,
	RIMCAS_UPDATE_BRIGHNESS = 8017,
	RIMCAS_UPDATE_FONTS = 8018,
	RIMCAS_UPDATE_LVP = 8019,
	RIMCAS_CA_MONITOR_FUNC = 8022,
	RIMCAS_CA_ARRIVAL_FUNC = 8020,
	RIMCAS_CLOSED_RUNWAYS_FUNC = 8021,
	RIMCAS_TAXIWAY_RUNWAYS_FUNC = 8023,
	RIMCAS_UPDATE_AFTERGLOW = 8030,
	RIMCAS_UPDATE_GND_TRAIL = 8031,
	RIMCAS_UPDATE_APP_TRAIL = 8032,
	RIMCAS_UPDATE_PTL = 8033, // Update predicted track line
	RIMCAS_UPDATE_RELEASE = 8034,
	RIMCAS_UPDATE_ACQUIRE = 8035,
	
	UPDATE_AIRBORNE_SV, // Inset speed vector
	
	RIMCAS_IAW = 7000,
	
	RIMCAS_UPDATERANGE = 6005,
	RIMCAS_UPDATEFILTER = 6015,
	RIMCAS_UPDATERANGE1 = 6006,
	RIMCAS_UPDATEFILTER1 = 6016,
	RIMCAS_UPDATERANGE2 = 6007,
	RIMCAS_UPDATEFILTER2 = 6017,
	
	RIMCAS_UPDATEROTATE = 6025,
	RIMCAS_UPDATEROTATE1 = 6026,
	RIMCAS_UPDATEROTATE2 = 6027,
	
	APPWINDOW_BASE = 8887,
	APPWINDOW_ONE = 8888,
	APPWINDOW_TWO = 8889,
	
	BELUX_SETPROMODE = 8890,
	BELUX_SETPROMODEEASY = 8891,
	
	FILTER_NON_ASSUMED = 8892,
	FILTER_SHOW_FREE = 8893,
	FILTER_SHOW_ON_BLOCKS = 8894,
	FILTER_SHOW_NSTS = 8895,
	FILTER_SHOW_STUP = 8896,

	// Brightness update
	RIMCAS_BRIGHTNESS_LABEL = 301,
	RIMCAS_BRIGHTNESS_SYMBOL = 302,
	RIMCAS_BRIGHTNESS_AFTERGLOW = 303,
	
	RIMCAS_DISTANCE_TOOL = 201,
};

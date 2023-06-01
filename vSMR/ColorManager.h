#pragma once
#include "stdafx.h"
#include <GdiPlus.h>
#include <map>
#include <string>

using namespace std;

#pragma once
class CColorManager
{
protected:
	map<string, int> ColorSettings{};
public:
	CColorManager()
	{
		update_brightness("label", 100);
		update_brightness("symbol", 70);
		update_brightness("afterglow", 100);
	};
	virtual ~CColorManager()
	{
		ColorSettings.clear();
	};

	void update_brightness(const string& id, const int value)
	{
		ColorSettings[id] = min(bounds_high(), max(bounds_low(), value));
	};

	int get_brightness(const string& id)
	{
		if (ColorSettings.find(id) != ColorSettings.end())
			return ColorSettings[id];
		return 100;
	};

	virtual Gdiplus::Color get_corrected_color(const string id, const Gdiplus::Color color)
	{
		if (get_brightness(id) == 100)
			return color;

		COLORREF colorref = color.ToCOLORREF();

		int r = GetRValue(colorref);
		int g = GetGValue(colorref);
		int b = GetBValue(colorref);
		const BYTE a = color.GetAlpha();
		

		r = min(255, int((r*get_brightness(id))/100));
		g = min(255, int((g*get_brightness(id))/100));
		b = min(255, int((b*get_brightness(id))/100));

		return Gdiplus::Color(a, r, g, b);
	};

	static int bounds_low()
	{
		return 70;
	};

	static int bounds_high()
	{
		return 130;
	};
};


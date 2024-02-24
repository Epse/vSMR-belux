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

	virtual Gdiplus::Color brighten(const BYTE brightness, const Gdiplus::Color color)
	{
		return {
			color.GetA(),
			min<BYTE>(255, static_cast<BYTE>((color.GetR() * brightness) / 100)),
			min<BYTE>(255, static_cast<BYTE>((color.GetG() * brightness) / 100)),
			min<BYTE>(255, static_cast<BYTE>((color.GetB() * brightness) / 100))
		};
	}

public:
	CColorManager()
	{
		update_brightness("label", 100);
		update_brightness("symbol", 100);
		update_brightness("target", 100);
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

	virtual Gdiplus::Color get_corrected_color(const string id, const int extra_dimming, const Gdiplus::Color color)
	{
		return this->brighten(max(0, get_brightness(id) - extra_dimming), color);
	}

	virtual Gdiplus::Color get_corrected_color(const string id, const Gdiplus::Color color)
	{
		if (get_brightness(id) == 100)
			return color;

		return this->brighten(get_brightness(id), color);
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

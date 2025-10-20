#pragma once
#include "EuroScopePlugIn.h"
#include <gdiplusgraphics.h>
#include <afxwin.h>
#include <string>
#include <map>
#include "rapidjson/document.h"

using namespace EuroScopePlugIn;

struct TagDrawingContext
{
	Gdiplus::Graphics* graphics;
	int line_height;
	const rapidjson::Value* labels_settings;
	const Gdiplus::SolidBrush* squawk_error_color;
	const Gdiplus::SolidBrush* rimcas_text_color;
	const Gdiplus::SolidBrush* ground_push_color;
	const Gdiplus::SolidBrush* ground_taxi_color;
	const Gdiplus::SolidBrush* ground_depa_color;
	const Gdiplus::SolidBrush* asel_text_color;
};
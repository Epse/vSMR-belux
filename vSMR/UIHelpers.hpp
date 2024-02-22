#pragma once
#include <GdiPlus.h>
#include "ColorManager.h"
#include "SMRRadar.hpp"
#include "TagTypes.hpp"

class UIHelper
{
public:
static string getEnumString(TagTypes type);
static void drawLeaderLine(const std::vector<PointF>& points, const PointF& acPos, const Gdiplus::Pen* pen, Gdiplus::Graphics* graphics);
};

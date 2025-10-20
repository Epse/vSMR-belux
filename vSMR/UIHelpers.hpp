#pragma once
#include <GdiPlus.h>
#include <optional>
#include "ColorManager.h"
#include "SMRRadar.hpp"
#include "TagTypes.hpp"

class UIHelper
{
public:
static string getEnumString(TagTypes type);
static void drawLeaderLine(const std::vector<PointF>& points, const PointF& acPos, const Gdiplus::Pen* pen, Gdiplus::Graphics* graphics);
static std::vector<PointF> grow_border(const std::vector<PointF>& border, const int growth, const bool right_align);
static std::optional<std::vector<std::vector<std::string>>> parse_label_lines(const Value& value);
static std::string altitude(const int x, const unsigned int transition = 6000);
static size_t id(const CRadarTarget& rt);
static size_t id(const CFlightPlan& fp);
};

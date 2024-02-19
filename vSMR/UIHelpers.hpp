#pragma once
#include <GdiPlus.h>
#include "ColorManager.h"
#include "SMRRadar.hpp"
#include "TagTypes.hpp"

class UIHelper
{
public:
static void drawAselBorder(Gdiplus::Graphics &graphics, CColorManager* colorManager, Rect tag);
static string getEnumString(TagTypes type);
};

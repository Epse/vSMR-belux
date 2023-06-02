#pragma once
#include <GdiPlus.h>
#include "ColorManager.h"
#include "SMRRadar.hpp"

class UIHelper
{
public:
static void drawAselBorder(Gdiplus::Graphics &graphics, CColorManager* colorManager, POINT tagCenter, int tagWidth, int tagHeight);
static string getEnumString(CSMRRadar::TagTypes type);
};
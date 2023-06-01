#pragma once
#include <GdiPlus.h>
#include "ColorManager.h"

class UIHelper
{
public:
static void drawAselBorder(Gdiplus::Graphics &graphics, CColorManager* colorManager, POINT tagCenter, int tagWidth, int tagHeight);
};
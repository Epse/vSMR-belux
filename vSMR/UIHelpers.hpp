#pragma once
#include <GdiPlus.h>
#include "ColorManager.h"

using namespace Gdiplus;

class UIHelper
{
public:
static void drawAselBorder(Graphics &graphics, CColorManager* colorManager, POINT tagCenter, int tagWidth, int tagHeight);
};
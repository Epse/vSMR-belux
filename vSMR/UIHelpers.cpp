#include "stdafx.h"
#include "UIHelpers.hpp"

void UIHelper::drawAselBorder(Graphics &graphics, CColorManager* colorManager, POINT tagCenter, int tagWidth, int tagHeight)
{
	const unsigned int border_width = 3; // Width of border. 4 is realistic-ish
	const unsigned int border_padding = 1; // pixels between tag and border. 2 is realistic-ish, maybe 1

	SolidBrush ASELBrush(colorManager->get_corrected_color("label", Color(255, 255, 0)));

	/*
	I don't exactly know why I have to subtract one pixel from the width
	of the top and bottom, but they are in fact one singular pixel too wide otherwise.

	It appears this may be because of rounding, as it happens to the height sometimes too
	*/

	// top
	graphics.FillRectangle(&ASELBrush, Rect(
		tagCenter.x - (tagWidth / 2) - border_width - border_padding, // x
		tagCenter.y - (tagHeight / 2) - border_width - border_padding, // y
		tagWidth + (border_width * 2) + (border_padding * 2) - 1, // w
		border_width // h
	));

	// bottom
	graphics.FillRectangle(&ASELBrush, Rect(
		tagCenter.x - (tagWidth / 2) - border_width - border_padding, // x
		tagCenter.y + (tagHeight / 2) + border_padding, // y
		tagWidth + (border_width * 2) + (border_padding * 2) - 1, // w
		border_width // h
	));

	// left
	graphics.FillRectangle(&ASELBrush, Rect(
		tagCenter.x - (tagWidth / 2) - border_width - border_padding, // x
		tagCenter.y - (tagHeight / 2) - border_width - border_padding, // y
		border_width, // w
		tagHeight + (2 * border_width) + (2 * border_padding) - 1
	));

	// right
	graphics.FillRectangle(&ASELBrush, Rect(
		tagCenter.x + (tagWidth / 2) + border_padding, // x
		tagCenter.y - (tagHeight / 2) - border_width - border_padding, // y
		border_width, // w
		tagHeight + (2 * border_width) + (2 * border_padding) - 1
	));
}

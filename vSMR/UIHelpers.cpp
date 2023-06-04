#include "stdafx.h"
#include "UIHelpers.hpp"

void UIHelper::drawAselBorder(Gdiplus::Graphics& graphics, CColorManager* colorManager, POINT tagCenter, int tagWidth, int tagHeight)
{
	constexpr unsigned int border_width = 3; // Width of border. 4 is realistic-ish
	constexpr unsigned int border_padding = 1; // pixels between tag and border. 1 is kinda realistic

	/* I have no clue about why I need to add a singular pixel...
	Assuming something with rounding, and I need to match whatever the actual tag is doing for it to look good. */

	const Gdiplus::Pen pen(colorManager->get_corrected_color("label", Gdiplus::Color::Yellow), border_width);
	graphics.DrawRectangle(&pen,
		static_cast<int>(tagCenter.x - (tagWidth / 2) - border_width - border_padding + 1), // x
		static_cast<int>(tagCenter.y - (tagHeight / 2) - border_width - border_padding + 1), // y
		tagWidth + (border_width)+border_padding * 2,
		tagHeight + (border_width)+border_padding * 2
	);
}

string UIHelper::getEnumString(TagTypes type) {
	if (type == TagTypes::Departure)
		return "departure";
	if (type == TagTypes::Arrival)
		return "arrival";
	if (type == TagTypes::Uncorrelated)
		return "uncorrelated";
	return "airborne";
}

#include "stdafx.h"
#include "UIHelpers.hpp"
#include <cmath>

string UIHelper::getEnumString(TagTypes type) {
	if (type == TagTypes::Departure)
		return "departure";
	if (type == TagTypes::Arrival)
		return "arrival";
	if (type == TagTypes::Uncorrelated)
		return "uncorrelated";
	return "airborne";
}

void UIHelper::drawLeaderLine(const std::vector<PointF>& points, const PointF& acPos, const Gdiplus::Pen* pen, Gdiplus::Graphics* graphics)
{

	PointF point;
	float distance = FLT_MAX;

	for (const PointF& p: points)
	{
		const float current_distance = sqrtf(powf(acPos.X - p.X, 2) + powf(acPos.Y - p.Y, 2));
		if (current_distance <= distance)
		{
			point = p;
			distance = current_distance;
		}
	}

	graphics->DrawLine(pen, acPos, point);
}

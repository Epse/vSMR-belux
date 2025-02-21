#include "stdafx.h"
#include "UIHelpers.hpp"
#include <cmath>

string UIHelper::getEnumString(TagTypes type)
{
	if (type == TagTypes::Departure)
		return "departure";
	if (type == TagTypes::Arrival)
		return "arrival";
	if (type == TagTypes::Uncorrelated)
		return "uncorrelated";
	return "airborne";
}

void UIHelper::drawLeaderLine(const std::vector<PointF>& points, const PointF& acPos, const Gdiplus::Pen* pen,
                              Gdiplus::Graphics* graphics)
{
	if (points.empty())
	{
		Logger::info("No points in drawLeaderLine");
		return;
	}

	PointF point = points[0];
	float distance = FLT_MAX;

	for (const PointF& p : points)
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

std::vector<PointF> UIHelper::grow_border(const std::vector<PointF>& border_points, const int growth,
                                          const bool right_align)
{
	std::vector<PointF> grown_points;
	grown_points.reserve(border_points.size());

	/*
	 * We need to define the polygon clockwise, so reverse iterate it if its counterclockwise
	 * Then, we can use some facts we know about this particular polygon to efficiently expand it.
	 */
	if (!right_align)
	{
		// The first point is the top left
		grown_points.emplace_back(border_points.front().X - growth, border_points.front().Y - growth);
		// Now, all but the last points need to expand +X
		for (size_t i = 1; i < border_points.size() - 1; ++i)
		{
			/*
			Y growth needs to be positive if
			- Last point was wider than current one
			- Next point is shorter than current one
			- We're the last point
			 */
			const int y_growth = (i > 0 && border_points[i - 1].X > border_points[i].X)
			                     || i == border_points.size() - 1
			                     || (i + 1 < border_points.size() - 1 && border_points[i + 1].X < border_points[i].X)
				                     ? +growth
				                     : -growth;
			grown_points.emplace_back(border_points[i].X + growth, border_points[i].Y + y_growth);
		}
		// But the penultimate point actually needs to grow into positive Y, so compensate
		grown_points.back().Y += 2 * growth;
		// And the last point needs negative X and positive Y
		grown_points.emplace_back(border_points.back().X - growth, border_points.back().Y + growth);
	}
	else
	{
		// We kinda do the same, but counterclockwise here
		grown_points.emplace_back(border_points.front().X + growth, border_points.front().Y - growth);
		for (size_t i = 1; i < border_points.size() - 1; ++i)
		{
			const int y_growth = (i > 0 && border_points[i - 1].X < border_points[i].X) // was last point wider?
			                     || i == border_points.size() - 1
			                     || (i + 1 < border_points.size() - 1 && border_points[i + 1].X > border_points[i].X)
				                     // Is next point shorter?
				                     ? +growth
				                     : -growth;
			grown_points.emplace_back(border_points[i].X - growth, border_points[i].Y + y_growth);
		}
		// The penultimate point needed positive Y, as above
		grown_points.back().Y += 2 * growth;
		// And the positive X and Y
		grown_points.emplace_back(border_points.back().X + growth, border_points.back().Y + growth);
	}
	return grown_points;
}

std::optional<std::vector<std::vector<std::string>>> UIHelper::parse_label_lines(const Value& value)
{
	if (!value.IsArray())
	{
		return {};
	}

	std::vector<std::vector<std::string>> output;
	output.reserve(value.Size());

	for (size_t i = 0; i < value.Size(); ++i)
	{
		if (!value[i].IsArray())
			return {};

		std::vector<std::string> elements;
		elements.reserve(value[i].Size());

		for (size_t j = 0; j < value[i].Size(); ++j)
		{
			if (!value[i][j].IsString())
				return {};
			elements.emplace_back(value[i][j].GetString());
		}

		output.emplace_back(elements);
	}

	return { output };
}


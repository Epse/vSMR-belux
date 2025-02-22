#pragma once
#include <vector>

#include "Constant.hpp"
#include "EuroScopePlugIn.h"

class PlaneShapeBuilder
{
public:
	static constexpr size_t patatoide_size = 11 * 7 + 6 + 1;
	static std::vector < CPosition > build(const EuroScopePlugIn::CRadarTargetPositionData &position, const EuroScopePlugIn::CFlightPlan &flight_plan, const bool randomise = true);
};


#pragma once
#include <filesystem>
#include <map>
#include <vector>

#include "Constant.hpp"
#include "EuroScopePlugIn.h"

struct AircraftType
{
	std::string type;
	float length;
	float tail_height;
	float width; // Actually main gear width.
};

class PlaneShapeBuilder
{
public:
	///	Number of points in resulting shape
	static constexpr size_t patatoide_size = 11 * 7 + 6 + 1;
	std::vector < CPosition > build(const EuroScopePlugIn::CRadarTargetPositionData &position, const EuroScopePlugIn::CFlightPlan &flight_plan, const bool randomise = true) const;
	void init();
private:
	bool initialized = false;
	std::map<std::string, AircraftType> types;
	size_t load_file(std::istream& str);
};


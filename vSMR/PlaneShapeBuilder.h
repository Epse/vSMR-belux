#pragma once
#include <filesystem>
#include <map>
#include <random>
#include <vector>

#include "Constant.hpp"
#include "EuroScopePlugIn.h"

struct AircraftType
{
	std::string type;
	float wingspan;
	float length;
	float tail_height;
	float width; // Actually main gear width.
};

class PlaneShapeBuilder
{
public:
	PlaneShapeBuilder();
	///	Number of points in resulting shape
	static constexpr size_t shape_size = 9;
	std::vector < CPosition > build(const EuroScopePlugIn::CRadarTargetPositionData &position, const EuroScopePlugIn::CFlightPlan &flight_plan, const bool randomise = true);
	void init();
private:
	std::default_random_engine ren;
	bool initialized = false;
	std::map<std::string, AircraftType> types;
	size_t load_file(std::istream& str);
};


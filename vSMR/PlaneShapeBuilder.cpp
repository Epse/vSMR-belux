#include "stdafx.h"
#include "PlaneShapeBuilder.h"
#include <fstream>
#include <random>

using namespace EuroScopePlugIn;

/*
 * Notes on the rework:
 * using https://www.faa.gov/airports/engineering/aircraft_char_database
 * we should be able to get actual aircraft sizes.
 * Then we load that from a file,
 * and have a noisy ellipse.
 *
 * An A320 or similar should be pretty much a blobby circle
 * whereas a B78X gets a more defined long shape, but with a similar width.
 *
 */

PlaneShapeBuilder::PlaneShapeBuilder()
{
	ren = std::default_random_engine{ std::random_device{}() };
}

std::vector < CPosition > PlaneShapeBuilder::build(const EuroScopePlugIn::CRadarTargetPositionData &position, const EuroScopePlugIn::CFlightPlan &flight_plan, const bool randomise)
{
	CPosition placeholder;
	placeholder.m_Latitude = 0.0f;
	placeholder.m_Longitude = 0.0f;
	std::vector<CPosition> result(shape_size, placeholder);

	if (!initialized)
	{
		return result;
	}


	AircraftType type{
		"C170",
		7.6f,2.0f,3.0f
	};

	if (flight_plan.IsValid())
	{
		const std::string icao = flight_plan.GetFlightPlanData().GetAircraftFPType();
		if (const auto found = types.find(icao); found != types.end())
		{
			type = found->second;
		}
	}

	/*
	 * Concept: Take a normal noise distribution (below).
	 * Assume this to be distance from centre.
	 * Sample this at a few angles around the centre.
	 * Done??
	 */
	std::normal_distribution distance_dist{ type.tail_height * 0.8f, type.tail_height / 5 };

	const auto heading = position.GetReportedHeadingTrueNorth();
	const CPosition centre = position.GetPosition();

	// For the reason of squeezing the middle a little... [0, 1[
	const float squeeze = 0.15f;

	for (size_t i = 0; i < shape_size; ++i)
	{
		/*
		 * We want to make this thing a tiny bit narrower around the waist, overall.
		 * We have N datapoints (shape_size, but N is shorter).
		 * Every N / 2 points, we effectively repeat the shape.
		 *
		 * 0 and N/2 should have full extent.
		 * N/4 and 3N/4 should be narrower by a bit.
		 *
		 * So, x=n/N gives a float [0, 1[. Yay no units.
		 * 
		 */
		const float angle = i * 360 / shape_size;
		const double real_angle = static_cast<int>(angle + heading) % 360;
		const auto distance = max(distance_dist(ren), 0);
		const auto mult = squeeze * sin(2 * DegToRad(angle) + PI / 2) + (1 - squeeze);
		result[i] = BetterHarversine(centre, static_cast<double>(real_angle), distance * mult);
	}

	return result;
}

void PlaneShapeBuilder::init()
{
	if (initialized) return;

	// Getting the DLL file folder
	char dll_path_file[MAX_PATH];
	GetModuleFileNameA(HINSTANCE(&__ImageBase), dll_path_file, sizeof(dll_path_file));
	std::filesystem::path aircraft_data_path(dll_path_file);
	aircraft_data_path.replace_filename("aircraft_data.tsv");

	{
		std::ifstream stream(aircraft_data_path);
		const auto loaded = load_file(stream);
		if (loaded < 1)
		{
			AfxMessageBox("Could not load a reasonable amount of aircraft types. Is the file present?");
		}
	}

	initialized = true;
}

size_t PlaneShapeBuilder::load_file(std::istream& str)
{
	types.clear();
	for (std::string line; std::getline(str, line);)
	{
		std::stringstream linestr(line);
		AircraftType type{};
		std::string item;

		std::getline(linestr, item, '\t');
		type.type = item;

		std::getline(linestr, item, '\t');
		type.wingspan = atof(item.c_str());

		std::getline(linestr, item, '\t');
		type.length = atof(item.c_str());

		std::getline(linestr, item, '\t');
		type.tail_height = atof(item.c_str());

		std::getline(linestr, item, '\t');
		type.width = atof(item.c_str());

		types[type.type] = type;
	}

	return types.size();
}

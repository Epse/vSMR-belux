#include "stdafx.h"
#include "PlaneShapeBuilder.h"
#include <fstream>

using namespace EuroScopePlugIn;

/*
 * Notes on the rework:
 * using https://www.faa.gov/airports/engineering/aircraft_char_database
 * we should be able to get actual aircraft sizes.
 * Then we load that from a file,
 * and have a noisy ellipse.
 */

std::vector < CPosition > PlaneShapeBuilder::build(const EuroScopePlugIn::CRadarTargetPositionData &position, const EuroScopePlugIn::CFlightPlan &flight_plan, const bool randomise) const
{
	if (!initialized)
	{
		throw logic_error("PlaneShapeBuilder not initialized.");
	}

	CPosition placeholder;
	placeholder.m_Latitude = 0.0f;
	placeholder.m_Longitude = 0.0f;

	// All units in M
	float width = 34.0f;
	float cabin_width = 4.0f;
	float length = 38.0f;

	if (flight_plan.IsValid())
	{
		const char wtc = flight_plan.GetFlightPlanData().GetAircraftWtc();

		if (wtc == 'L')
		{
			width = 13.0f;
			cabin_width = 2.0f;
			length = 12.0f;
		}

		if (wtc == 'H')
		{
			width = 61.0f;
			cabin_width = 7.0f;
			length = 64.0f;
		}

		if (wtc == 'J')
		{
			width = 80.0f;
			cabin_width = 7.0f;
			length = 73.0f;
		}
	}


	if (randomise)
	{
		width = width + float((rand() % 5) - 2);
		cabin_width = cabin_width + float((rand() % 3) - 1);
		length = length + float((rand() % 5) - 2);
	}


	const auto trackHead = float(position.GetReportedHeadingTrueNorth());
	const auto inverseTrackHead = float(fmod(trackHead + 180.0f, 360));
	const auto leftTrackHead = float(fmod(trackHead - 90.0f, 360));
	const auto rightTrackHead = float(fmod(trackHead + 90.0f, 360));

	const float HalfLenght = length / 2.0f;
	const float HalfCabWidth = cabin_width / 2.0f;
	const float HalfSpanWidth = width / 2.0f;

	// Base shape is like a deformed cross


	const CPosition topMiddle = Haversine(position.GetPosition(), trackHead, HalfLenght);
	const CPosition topLeft = Haversine(topMiddle, leftTrackHead, HalfCabWidth);
	const CPosition topRight = Haversine(topMiddle, rightTrackHead, HalfCabWidth);

	const CPosition bottomMiddle = Haversine(position.GetPosition(), inverseTrackHead, HalfLenght);
	const CPosition bottomLeft = Haversine(bottomMiddle, leftTrackHead, HalfCabWidth);
	const CPosition bottomRight = Haversine(bottomMiddle, rightTrackHead, HalfCabWidth);

	const CPosition middleTopLeft = Haversine(topLeft, float(fmod(inverseTrackHead + 25.0f, 360)), 0.8f * HalfLenght);
	const CPosition middleTopRight = Haversine(topRight, float(fmod(inverseTrackHead - 25.0f, 360)), 0.8f * HalfLenght);
	const CPosition middleBottomLeft = Haversine(bottomLeft, float(fmod(trackHead - 15.0f, 360)), 0.8f * HalfLenght);
	const CPosition middleBottomRight = Haversine(bottomRight, float(fmod(trackHead + 15.0f, 360)), 0.8f * HalfLenght);

	const CPosition rightTop = Haversine(middleBottomRight, rightTrackHead, 0.7f * HalfSpanWidth);
	const CPosition rightBottom = Haversine(rightTop, inverseTrackHead, cabin_width);

	const CPosition leftTop = Haversine(middleBottomLeft, leftTrackHead, 0.7f * HalfSpanWidth);
	const CPosition leftBottom = Haversine(leftTop, inverseTrackHead, cabin_width);

	CPosition basePoints[12];
	basePoints[0] = topLeft;
	basePoints[1] = middleTopLeft;
	basePoints[2] = leftTop;
	basePoints[3] = leftBottom;
	basePoints[4] = middleBottomLeft;
	basePoints[5] = bottomLeft;
	basePoints[6] = bottomRight;
	basePoints[7] = middleBottomRight;
	basePoints[8] = rightBottom;
	basePoints[9] = rightTop;
	basePoints[10] = middleTopRight;
	basePoints[11] = topRight;

	if (!randomise)
	{
		return {std::begin(basePoints), std::end(basePoints)};
	}

	// 12 points total, so 11 from 0
	// ------

	// Random points between points of base shape

    std::vector<CPosition> result(patatoide_size, placeholder);
	for (int i = 0; i < 12; i++)
	{
		CPosition lastPoint, endPoint, startPoint;

		startPoint = basePoints[i];
		if (i == 11) endPoint = basePoints[0];
		else endPoint = basePoints[i + 1];

		const double dist = startPoint.DistanceTo(endPoint);

		result[i * 7] = startPoint;
		lastPoint = startPoint;

		for (int k = 1; k < 7; k++)
		{
			const double rndHeading = float(fmod(lastPoint.DirectionTo(endPoint) + (-25.0 + (rand() % 50 + 1)), 360));
			const CPosition newPoint = Haversine(lastPoint, rndHeading, dist * 200);
			result[(i * 7) + k] = newPoint;
			lastPoint = newPoint;
		}
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
		load_file(stream);
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

		std::getline(linestr, line, '\t');
		type.length = atof(item.c_str());

		std::getline(linestr, line, '\t');
		type.tail_height = atof(item.c_str());

		std::getline(linestr, line, '\t');
		type.width = atof(item.c_str());

		types[type.type] = type;
	}

	return types.size();
}

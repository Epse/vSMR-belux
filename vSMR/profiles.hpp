#pragma once
#include <EuroScopePlugIn.h>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>
#include <rapidjson/document.h>

#include "TagTypes.hpp"

#define ensure_has_int(_v, __f, msg) if (!(_v).HasMember(__f) || !(_v)[__f].IsInt()) throw std::runtime_error(msg)
#define ensure_has_string(_v, __f, msg) if (!(_v).HasMember(__f) || !(_v)[__f].IsString()) throw std::runtime_error(msg)
#define ensure_has_array(_v, __f, msg) if (!(_v).HasMember(__f) || !(_v)[__f].IsArray()) throw std::runtime_error(msg);
#define ensure_has_object(_v, __f, msg) if (!(_v).HasMember(__f) || !(_v)[__f].IsObject()) throw std::runtime_error(msg);

#define get_string_or_except(value, field, error_message) ((!(value).HasMember(field) || !(value)[field].IsString()) ? throw std::runtime_error(error_message) : (value)[field].GetString())
#define get_int_or_except(value, field, error_message) ((!(value).HasMember(field) || !(value)[field].IsInt()) ? throw std::runtime_error(error_message) : (value)[field].GetInt())
#define get_uint_or_except(value, field, error_message) ((!(value).HasMember(field) || !(value)[field].IsInt()) ? throw std::runtime_error(error_message) : clamp_uint((value)[field].GetInt()))
#define get_byte_or_except(value, field, error_message) ((!(value).HasMember(field) || !(value)[field].IsInt()) ? throw std::runtime_error(error_message) : clamp_byte((value)[field].GetInt()))
#define get_bool_or_except(value, field, error_message) ((!(value).HasMember(field) || !(value)[field].IsBool()) ? throw std::runtime_error(error_message) : (value)[field].GetBool())
#define get_object_or_except(value, field, error_message) ((!(value).HasMember(field) || !(value)[field].IsObject()) ? throw std::runtime_error(error_message) : (value)[field].GetObj())
#define get_array_or_except(value, field, error_message) ((!(value).HasMember(field) || !(value)[field].IsArray()) ? throw std::runtime_error(error_message) : (value)[field].GetArray())

using EuroScopePlugIn::CPosition;

constexpr size_t font_sizes = 5;

struct FontConfig
{
	std::string font_name;
	unsigned int title_offset;
	unsigned int sizes[font_sizes]; // Deviation from og files
};

struct ProModeConfig
{
	bool enable;
	bool accept_pilot_squawk;
	std::vector<std::string> do_not_autocorrelate_squawks; // Only God and Gergely know why squawk is a string
};

struct FilterConfig
{
	int hide_above_alt;
	int hide_above_speed;
	unsigned int radar_range_nm;
	BYTE night_alpha_setting;
	ProModeConfig pro_mode;
};

struct TagConfig
{
	std::vector<std::vector<std::string>> definition;
	Gdiplus::Color background_color;
	Gdiplus::Color text_color;
	Gdiplus::Color background_color_on_runway;
};

struct DepartureConfig : public TagConfig
{
	Gdiplus::Color nofpl_color;
	Gdiplus::Color nosid_color;
};

struct AirborneConfig : public TagConfig
{
	bool use_departure_arrival_coloring;
};

struct GroundStatusColors
{
	Gdiplus::Color push;
	Gdiplus::Color taxi;
	Gdiplus::Color depa;
};

struct LabelConfig
{
	bool auto_deconfliction;
	bool use_aspeed_for_gate;
	Gdiplus::Color squawk_error_color;
	GroundStatusColors groundstatus_colors;

	DepartureConfig departure;
	TagConfig arrival;
	AirborneConfig airborne;
	TagConfig uncorrelated;

	const std::vector<std::vector<std::string>>& lines(const TagTypes& type) const
	{
		switch (type)
		{
		case TagTypes::Airborne:
			return airborne.definition;
		case TagTypes::Arrival:
			return arrival.definition;
		case TagTypes::Departure:
			return departure.definition;
		case TagTypes::Uncorrelated:
			return uncorrelated.definition;
		}
	}

	Gdiplus::Color background_color(const TagTypes& type) const
	{
		switch (type)
		{
		case TagTypes::Airborne:
			return airborne.background_color;
		case TagTypes::Arrival:
			return arrival.background_color;
		case TagTypes::Departure:
			return departure.background_color;
		case TagTypes::Uncorrelated:
			return uncorrelated.background_color;
		}
	}

	Gdiplus::Color background_color_on_runway(const TagTypes& type) const
	{
		switch (type)
		{
		case TagTypes::Airborne:
			return airborne.background_color_on_runway;
		case TagTypes::Arrival:
			return arrival.background_color_on_runway;
		case TagTypes::Departure:
			return departure.background_color_on_runway;
		case TagTypes::Uncorrelated:
			return uncorrelated.background_color_on_runway;
		}
	}

	Gdiplus::Color text_color(const TagTypes& type) const
	{
		switch (type)
		{
		case TagTypes::Airborne:
			return airborne.text_color;
		case TagTypes::Arrival:
			return arrival.text_color;
		case TagTypes::Departure:
			return departure.text_color;
		case TagTypes::Uncorrelated:
			return uncorrelated.text_color;
		}
	}
};

struct RimcasConfig
{
	bool rimcas_label_only;
	std::vector<unsigned int> timer;
	std::vector<unsigned int> timer_lvp;
	unsigned int stage_two_speed_threshold;
	Gdiplus::Color background_color_stage_one;
	Gdiplus::Color background_color_stage_two;
	Gdiplus::Color alert_text_color;
};

struct AirborneTargetConfig
{
	Gdiplus::Color problem;
	Gdiplus::Color assumed;
	Gdiplus::Color unassumed;
};

struct TargetConfig
{
	bool show_primary_target;
	AirborneTargetConfig airborne;
	Gdiplus::Color target_color;
	Gdiplus::Color history_one_color;
	Gdiplus::Color history_two_color;
	Gdiplus::Color history_three_color;
};

struct ApproachInsetsConfig
{
	unsigned int extended_lines_length;
	unsigned int extended_lines_tick_spacing;
	Gdiplus::Color extended_lines_color;
	Gdiplus::Color runway_color;
	Gdiplus::Color background_color;
};

struct Sid
{
	std::vector<std::string> names;
	Gdiplus::Color color;
};

struct Runway
{
	std::string runway_name;
	std::vector<CPosition> path;
	std::vector<CPosition> path_lvp;
};

struct MapConfig
{
	std::vector<Sid> sids;
	std::vector<Runway> runways;
};

struct Profile
{
	std::string name;
	FontConfig font;
	FilterConfig filters;
	LabelConfig labels;
	RimcasConfig rimcas;
	TargetConfig targets;
	ApproachInsetsConfig approach_insets;
	std::map<std::string, MapConfig> maps;
};

class ProfileParser
{
public:
	/**
	 * Parses the given string into a provided vector of profiles, returning a string on error
	 * @param input JSON text to parse as vector of profiles
	 * @param out Destination vector to emplace profiles into. Contents are indeterminate in the error case.
	 * @return error text or empty optional on success
	 */
	static std::optional<std::string> parse(const std::string& input, std::vector<Profile>& out);

private:
	static unsigned int clamp_uint(int n)
	{
		return n < 0 ? 0 : static_cast<unsigned int>(n);
	}

	static BYTE clamp_byte(int n)
	{
		n = n > 255 ? 255 : n;
		return n < 0 ? 0 : static_cast<BYTE>(n);
	}

	// The below parser methods throw on error

	static Profile parse_profile(const rapidjson::Value& value);

	static std::vector<std::string> parse_strings(const rapidjson::Value& value);

	static std::vector<unsigned int> parse_uints(const rapidjson::Value& value);

	static Gdiplus::Color parse_color(const rapidjson::Value& value);

	static TagConfig parse_tag_config(const rapidjson::Value& value);

	static FontConfig parse_font(const rapidjson::Value& value);

	static FilterConfig parse_filter(const rapidjson::Value& value);

	static LabelConfig parse_labels(const rapidjson::Value& value);

	static RimcasConfig parse_rimcas(const rapidjson::Value& value);

	static AirborneTargetConfig parse_airborne_targets(const rapidjson::Value& value);

	static TargetConfig parse_targets(const rapidjson::Value& value);

	static ApproachInsetsConfig parse_insets(const rapidjson::Value& value);

	static Sid parse_sid(const rapidjson::Value& value);

	static std::vector<CPosition> parse_path(const rapidjson::Value& value);

	static Runway parse_runway(const rapidjson::Value& value);

	static MapConfig parse_map(const rapidjson::Value& value);

	static std::map<std::string, MapConfig> parse_maps(const rapidjson::Value& value);
};

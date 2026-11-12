#include "stdafx.h"
#include "profiles.hpp"
#include <gdipluscolor.h>

std::optional<std::string> ProfileParser::parse(const std::string& input, std::vector<Profile>& out)
{
	rapidjson::Document document;
	document.Parse(input.c_str());
	if (!document.IsArray()) return {std::string("Profiles file must contain an array of profiles")};

	for (auto& profile : document.GetArray())
	{
		try
		{
			const auto r = parse_profile(profile);
			out.emplace_back(r);
		}
		catch (std::exception& exception)
		{
			return {exception.what()};
		}
	}
	return {};
}

Profile ProfileParser::parse_profile(const rapidjson::Value& value)
{
	const std::string name = !value.HasMember("name")
		                         ? throw std::runtime_error("Profile contains no name")
		                         : value["name"].GetString();

	return {
		name,
		parse_font(get_object_or_except(value, "font", "font field missing from profile")),
		parse_filter(get_object_or_except(value, "filters", "filters field missing from profile")),
		parse_labels(get_object_or_except(value, "labels", "labels field missing from profile")),
		parse_rimcas(get_object_or_except(value, "rimcas", "rimcas field missing from profile")),
		parse_targets(get_object_or_except(value, "targets", "targets field missing from profile")),
		parse_insets(get_object_or_except(value, "approach_insets", "approach_insets field missing from profile")),
		parse_maps(get_object_or_except(value, "maps", "maps field missing from profile"))
	};
}

std::vector<std::string> ProfileParser::parse_strings(const rapidjson::Value& value)
{
	if (!value.IsArray()) throw std::runtime_error("Expected array, got something else");

	std::vector<std::string> items;
	for (auto& item : value.GetArray())
	{
		if (!item.IsString()) throw std::runtime_error("Expected string array, got something else");
		items.emplace_back(item.GetString());
	}
	return items;
}

std::vector<unsigned int> ProfileParser::parse_uints(const rapidjson::Value& value)
{
	if (!value.IsArray()) throw std::runtime_error("Expected array of numbers, got something else");

	std::vector<unsigned int> items;
	for (auto& item : value.GetArray())
	{
		if (!item.IsInt()) throw std::runtime_error("Expected array of numbers, got something else");
		items.emplace_back(clamp_uint(item.GetInt()));
	}
	return items;
}

Gdiplus::Color ProfileParser::parse_color(const rapidjson::Value& value)
{
	if (!value.IsObject()) throw std::exception("Color is not object");
	if (!value.HasMember("r") || !value["r"].IsInt()) throw std::runtime_error("Color missing r channel");
	if (!value.HasMember("g") || !value["r"].IsInt()) throw std::runtime_error("Color missing g channel");
	if (!value.HasMember("b") || !value["b"].IsInt()) throw std::runtime_error("Color missing b channel");

	if (!value.HasMember("a"))
	{
		return {clamp_byte(value["r"].GetInt()), clamp_byte(value["g"].GetInt()), clamp_byte(value["b"].GetInt())};
	}
	if (!value["a"].IsInt()) throw std::exception("Color a channel not numeric");
	return {
		clamp_byte(value["a"].GetInt()), clamp_byte(value["r"].GetInt()), clamp_byte(value["g"].GetInt()),
		clamp_byte(value["b"].GetInt())
	};
}

TagConfig ProfileParser::parse_tag_config(const rapidjson::Value& value)
{
	if (!value.IsObject()) throw std::runtime_error("Tag definition is not an object");

	std::vector<std::vector<std::string>> definition;

	for (auto& row : get_array_or_except(value, "definition", "tag requires definition"))
	{
		if (!row.IsArray()) throw std::runtime_error("Tag definition rows must be string arrays");
		std::vector<std::string> r;
		for (auto& elem : row.GetArray())
		{
			if (!elem.IsString()) throw std::runtime_error("Tag definition entries must be string");
			r.emplace_back(elem.GetString());
		}
		definition.emplace_back(r);
	}

	return {
		.definition = definition,
		.background_color = parse_color(
			get_object_or_except(value, "background_color", "tag requires background_color")),
		.text_color = parse_color(get_object_or_except(value, "text_color", "tag requires text color")),
		.background_color_on_runway = parse_color(
			get_object_or_except(value, "background_color_on_runway", "tag requires background_color_on_runway")),
	};
}

FontConfig ProfileParser::parse_font(const rapidjson::Value& value)
{
	ensure_has_string(value, "font_name", "Font is missing font_name");
	const std::string font_name = value["font_name"].GetString();

	ensure_has_int(value, "title_offset", "Font is missing title_offset");
	const auto title_offset = clamp_uint(value["title_offset"].GetInt());

	ensure_has_array(value, "sizes", "Font is missing sizes array");
	const auto sizes_array = value["sizes"].GetArray();
	if (sizes_array.Size() < 5) throw std::runtime_error("Five font sizes required");

	FontConfig font{.font_name = font_name, .title_offset = title_offset};
	for (size_t i = 0; i < font_sizes; ++i)
	{
		const auto& item = sizes_array[i];
		if (!item.IsInt()) throw std::runtime_error("Font size value is not numeric");
		font.sizes[i] = clamp_uint(item.GetInt());
	}
	return font;
}

FilterConfig ProfileParser::parse_filter(const rapidjson::Value& value)
{
	const auto pro_obj = get_object_or_except(value, "pro_mode", "filters missing pro_mode entry");
	const ProModeConfig pm{
		.enable = get_bool_or_except(pro_obj, "enable", "pro mode missing enable"),
		.accept_pilot_squawk = get_bool_or_except(pro_obj, "accept_pilot_squawk",
		                                          "pro_mode missing accept_pilot_squawk"),
		.do_not_autocorrelate_squawks = parse_strings(get_array_or_except(
			pro_obj, "do_not_autocorrelate_squawks", "Expected string array do_not_autocorrelate_squawks in filters"))
	};

	return {
		.hide_above_alt = get_int_or_except(value, "hide_above_alt",
		                                    "Filters missing hide_above_alt or not numeric"),
		.hide_above_speed = get_int_or_except(value, "hide_above_spd",
		                                      "Filters hide_above_spd missing or not numeric"),
		.radar_range_nm = get_uint_or_except(value, "radar_range_nm",
		                                     "Filters radar_range_nm missing or not numeric"),
		.night_alpha_setting = get_byte_or_except(value, "night_alpha_setting",
		                                          "Filters night_alpha_setting missing or not numeric"),
		.pro_mode = pm,
	};
}

LabelConfig ProfileParser::parse_labels(const rapidjson::Value& value)
{
	if (!value.IsObject()) throw std::runtime_error("Expected labels config to be object");

	const auto& gstatus_obj = get_object_or_except(value, "groundstatus_colors", "Expected groundstatus_colors");
	GroundStatusColors groundstatus_colors{
		.push = parse_color(get_object_or_except(gstatus_obj, "push", "Groundstatus_colors requires push")),
		.taxi = parse_color(get_object_or_except(gstatus_obj, "taxi", "Groundstatus_colors requires taxi")),
		.depa = parse_color(get_object_or_except(gstatus_obj, "depa", "Groundstatus_colors requires depa")),
	};

	const auto dep_obj = get_object_or_except(value, "departure", "labels requires departure field");
	const auto air_obj = get_object_or_except(value, "airborne", "labels requires airborne field");
	const TagConfig departure = parse_tag_config(dep_obj);
	const TagConfig airborne = parse_tag_config(air_obj);

	return {
		.auto_deconfliction = get_bool_or_except(value, "auto_deconfliction",
		                                         "Expected bool auto_deconfliction in labels"),
		.use_aspeed_for_gate = get_bool_or_except(value, "use_aspeed_for_gate",
		                                          "Expected bool use_aspeed_for_gate in labels"),
		.squawk_error_color = parse_color(
			get_object_or_except(value, "squawk_error_color",
			                     "Expected color object squawk_error_color in labels")),
		.groundstatus_colors = groundstatus_colors,
		.departure = {
			departure,
			parse_color(get_object_or_except(dep_obj, "nofpl_color", "tag departure requires nofpl_color")),
			parse_color(get_object_or_except(dep_obj, "nosid_color", "tag departure requires nosid_color")),
		},
		.arrival = parse_tag_config(get_object_or_except(value, "arrival", "labels requires arrival field")),
		.airborne = {
			airborne,
			get_bool_or_except(air_obj, "use_departure_arrival_coloring",
			                   "airborne label requires use_departure_arrival_coloring")
		},
		.uncorrelated = parse_tag_config(
			get_object_or_except(value, "uncorrelated", "labels requires uncorrelated field")),
	};
}

RimcasConfig ProfileParser::parse_rimcas(const rapidjson::Value& value)
{
	if (!value.IsObject()) throw std::runtime_error("Expected rimcas config to be object");

	return {
		.rimcas_label_only = get_bool_or_except(value, "rimcas_label_only", "Expected bool rimcas_label_only"),
		.timer = parse_uints(get_array_or_except(value, "timer", "Expected rimcas timer list")),
		.timer_lvp = parse_uints(get_array_or_except(value, "timer_lvp", "Expected rimcas timer_lvp list")),
		.stage_two_speed_threshold = get_uint_or_except(value, "rimcas_stage_two_speed_threshold",
		                                                "Expected rimcas_stage_two_speed_threshold"),
		.background_color_stage_one = parse_color(
			get_object_or_except(value, "background_color_stage_one",
			                     "Rimcas expected background_color_stage_one")),
		.background_color_stage_two = parse_color(
			get_object_or_except(value, "background_color_stage_two",
			                     "Rimcas expected background_color_stage_two")),
		.alert_text_color = parse_color(
			get_object_or_except(value, "alert_text_color", "Rimcas expected alert_text_color"))
	};
}

AirborneTargetConfig ProfileParser::parse_airborne_targets(const rapidjson::Value& value)
{
	if (!value.IsObject()) throw std::runtime_error("Expected airborne targets to be object");

	return {
		.problem = parse_color(get_object_or_except(value, "problem", "Expected problem color in targets.airborne")),
		.assumed = parse_color(get_object_or_except(value, "assumed", "Expected assumed color in targets.airborne")),
		.unassumed = parse_color(get_object_or_except(value, "unassumed",
		                                              "Expected unassumed color in targets.airborne")),
		.incoming_coordination = parse_color(get_object_or_except(value, "incoming_coordination",
		                                                          "Expected incoming_coordination color in targets.airborne")),
	};
}

TargetConfig ProfileParser::parse_targets(const rapidjson::Value& value)
{
	if (!value.IsObject()) throw std::runtime_error("Expected targets config to be object");

	return {
		.show_primary_target = get_bool_or_except(value, "show_primary_target", "Targets expected show_primary_target"),
		.airborne = parse_airborne_targets(get_object_or_except(value, "airborne", "Targets expected airborne key")),
		.target_color = parse_color(get_object_or_except(value, "target_color", "Targets expected target_color")),
		.history_one_color = parse_color(
			get_object_or_except(value, "history_one_color", "Targets expected history_one_color")),
		.history_two_color = parse_color(
			get_object_or_except(value, "history_two_color", "Targets expected history_two_color")),
		.history_three_color = parse_color(
			get_object_or_except(value, "history_three_color", "Targets expected history_three_color")),
	};
}

ApproachInsetsConfig ProfileParser::parse_insets(const rapidjson::Value& value)
{
	if (!value.IsObject()) throw std::runtime_error("Expected insets config to be object");

	return {
		.extended_lines_length = get_uint_or_except(value, "extended_lines_length",
		                                            "Approach insets expected extended_lines_length"),
		.extended_lines_tick_spacing = get_uint_or_except(value, "extended_lines_ticks_spacing",
		                                                  "Approach insets expected extended_lines_ticks_spacing"),
		.extended_lines_color = parse_color(
			get_object_or_except(value, "extended_lines_color", "Approach insets expected extended_lines_color")),
		.runway_color = parse_color(
			get_object_or_except(value, "runway_color", "Approach insets expected runway_color")),
		.background_color = parse_color(
			get_object_or_except(value, "background_color", "Approach insets expected background_color")),
		.fixes_visual =
		parse_strings(get_array_or_except(value, "fixes_visual", "Insets config requires fixes_visual")),
		.fixes_instrument = parse_strings(
			get_array_or_except(value, "fixes_instrument", "Insets config requires fixes_instrument")),
		.default_layers = value.HasMember("default_layers")
			                  ? parse_layers(get_object_or_except(value, "default_layers", "default layers if present must be object"))
			                  : Layers::init(),
	};
}

Sid ProfileParser::parse_sid(const rapidjson::Value& value)
{
	if (!value.IsObject()) throw std::runtime_error("Expected sid to be object");

	return {
		.names = parse_strings(get_array_or_except(value, "names", "Sid entry expected string array for names")),
		.color = parse_color(get_object_or_except(value, "color", "Sid entry expected color in color field"))
	};
}

std::vector<CPosition> ProfileParser::parse_path(const rapidjson::Value& value)
{
	if (!value.IsArray()) throw std::runtime_error("Expected runway path to be array");

	std::vector<CPosition> path;
	for (auto& point : value.GetArray())
	{
		if (!point.IsArray()) throw std::runtime_error("Path entry must be array");
		const auto coords = point.GetArray();
		if (coords.Size() != 2) throw std::runtime_error("Path entry must be 2 entry coordinate array");
		CPosition x;
		const auto success = x.LoadFromStrings(coords[1].GetString(), coords[0].GetString());
		if (!success) throw std::runtime_error("Could not parse path point, did you flip latitude and longitude?");
		path.emplace_back(x);
	}

	return path;
}

Runway ProfileParser::parse_runway(const rapidjson::Value& value)
{
	if (!value.IsObject()) throw std::runtime_error("Expected runway to be object");

	return {
		.runway_name = get_string_or_except(value, "runway_name", "Runway entry expected string runway_name"),
		.path = parse_path(get_array_or_except(value, "path", "Runway entry expected path")),
		.path_lvp = parse_path(get_array_or_except(value, "path_lvp", "Runway entry expected path_lvp")),
	};
}

MapConfig ProfileParser::parse_map(const rapidjson::Value& value)
{
	if (!value.IsObject()) throw std::runtime_error("Expected map entry to be object");

	std::vector<Sid> sids;
	for (const auto& item : get_array_or_except(value, "sids", "Map item requires sids"))
	{
		sids.emplace_back(parse_sid(item));
	}

	std::vector<Runway> runways;
	for (const auto& item : get_array_or_except(value, "runways", "Map item requires runways"))
	{
		runways.emplace_back(parse_runway(item));
	}

	return {
		sids, runways
	};
}

std::map<std::string, MapConfig> ProfileParser::parse_maps(const rapidjson::Value& value)
{
	if (!value.IsObject()) throw std::runtime_error("Expected maps config to be object");

	std::map<std::string, MapConfig> maps;
	for (const auto& kv : value.GetObj())
	{
		// Key always string in json
		maps[kv.name.GetString()] = parse_map(kv.value);
	}

	return maps;
}

Layers ProfileParser::parse_layers(const rapidjson::Value& value)
{
	return {
		.vfr_fixes = get_bool_or_true(value, "vfr_fixes", "vfr_fixes should be bool"),
		.ifr_fixes = get_bool_or_true(value, "ifr_fixes", "ifr_fixes should be bool"),
		.ctr = get_bool_or_true(value, "ctr", "ctr should be bool"),
		.tma = get_bool_or_true(value, "tma", "tma should be bool"),
		.ranges_1 = get_bool_or_true(value, "ranges_1", "ranges_1 should be bool"),
		.ranges_2 = get_bool_or_true(value, "ranges_2", "ranges_2 should be bool"),
	};
}

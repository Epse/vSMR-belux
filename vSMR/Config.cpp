#include "stdafx.h"
#include "Config.hpp"
#include <algorithm>
#include <fstream>

#include "profiles.hpp"
#include <sstream>

CConfig::CConfig(const string& configPath)
{
	config_path = configPath;
	loadConfig();
	active_profile = &profiles[0];

	set_active_profile("Default");
}

void CConfig::reload()
{
	const auto current_profile_name = active_profile->name;
	loadConfig();

	for (size_t i = 0; i < profiles.size(); ++i)
	{
		if (profiles[i].name == current_profile_name)
		{
			active_profile = &profiles[i];
			return;
		}
	}

	active_profile = &profiles[0];
}

void CConfig::loadConfig()
{
	stringstream ss;
	ifstream ifs;
	ifs.open(config_path.c_str(), std::ios::binary);
	ss << ifs.rdbuf();
	ifs.close();

	profiles.clear();

	const auto err = ProfileParser::parse(ss.str(), profiles);

	if (err.has_value())
	{
		const std::string msg = "Config read error: " + err.value();
		AfxMessageBox(msg.c_str(), MB_OK);

		ASSERT(AfxGetMainWnd() != NULL);
		AfxGetMainWnd()->SendMessage(WM_CLOSE);
	}

	if (profiles.empty())
	{
		AfxMessageBox("Got no profiles in config parsing. Exiting now.", MB_OK);

		ASSERT(AfxGetMainWnd() != NULL);
		AfxGetMainWnd()->SendMessage(WM_CLOSE);
	}

	active_profile = &profiles[0];
}

std::optional<Gdiplus::Color> CConfig::sid_color(const string& sid, const string& airport) const
{
	if (!active_profile->maps.contains(airport)) return {};

	for (auto& value : active_profile->maps[airport].sids)
	{
		if (ranges::find(value.names, sid) == value.names.end()) continue;

		return {value.color};
	}

	return {};
}

std::optional<MapConfig> CConfig::get_airport_map_if_any(const string& airport) const
{
	if (!active_profile->maps.contains(airport)) return std::nullopt;
	return active_profile->maps[airport];
}

bool CConfig::is_airport_map_avail(const string& airport) const
{
	return active_profile->maps.contains(airport);
}

bool CConfig::is_custom_cursor_used()
{
	return true;
	// by default use custom one so we don't break compatibility for old json settings that don't have the entry
}

std::optional<Runway> CConfig::custom_runway(const string& airport, const string& name1, const string& name2) const
{
	if (!active_profile->maps.contains(airport))
		return std::nullopt;

	const auto& rwys = active_profile->maps[airport].runways;
	for (const auto& rwy : rwys)
	{
		if (rwy.runway_name.starts_with(name1) || rwy.runway_name.starts_with(name2)) return { rwy };
	}
	return std::nullopt;
}

vector<string> CConfig::all_profile_names() const
{
	vector<string> toR;
	for (const auto& p : profiles)
	{
		toR.push_back(p.name);
	}
	return toR;
}

CConfig::~CConfig() = default;

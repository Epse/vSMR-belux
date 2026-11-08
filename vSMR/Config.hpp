#pragma once
#define NOMINMAX // windows.h defines min/max as macros, which conflicts... lol
#include <string>
#include <vector>
#include <Gdiplus.h>
#include "profiles.hpp"

using namespace std;

class CConfig
{
public:
	CConfig(const string& configPath);
	virtual ~CConfig();

	const Profile& getActiveProfile() const
	{
		return *active_profile;
	}
	const std::string& getActiveProfileName() const
	{
		return active_profile->name;
	}
	std::optional<Gdiplus::Color> sid_color(const string& sid, const string& airport) const;
	std::optional<MapConfig> get_airport_map_if_any(const string& airport) const;
	bool is_airport_map_avail(const string& airport) const;
	std::optional<Runway> custom_runway(const string& airport, const string& name1, const string& name2) const;
	bool is_custom_cursor_used();

	vector<string> all_profile_names() const;

	inline void set_active_profile(const string& newProfile)
	{
		for (auto& profile : profiles)
		{
			if (profile.name == newProfile) active_profile = &profile;
		}
	}

	const Profile& operator*() const
	{
		return *active_profile;
	}

protected:
	string config_path;
	Profile* active_profile;
	vector<Profile> profiles;

	void loadConfig();
};

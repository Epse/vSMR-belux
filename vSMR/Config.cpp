#include "stdafx.h"
#include "Config.hpp"
#include <algorithm>

CConfig::CConfig(const string& configPath)
{
	config_path = configPath;
	loadConfig();

	setActiveProfile("Default");
}

void CConfig::loadConfig() {

	stringstream ss;
	ifstream ifs;
	ifs.open(config_path.c_str(), std::ios::binary);
	ss << ifs.rdbuf();
	ifs.close();

	if (document.Parse<0>(ss.str().c_str()).HasParseError()) {
		AfxMessageBox("An error parsing vSMR configuration occurred.\nOnce fixed, reload the config by typing '.smr reload'", MB_OK);

		ASSERT(AfxGetMainWnd() != NULL);
		AfxGetMainWnd()->SendMessage(WM_CLOSE);
	}

	profiles.clear();

	assert(document.IsArray());

	for (SizeType i = 0; i < document.Size(); i++) {
		const Value& profile = document[i];
		string profile_name = profile["name"].GetString();

		profiles.insert(pair<string, rapidjson::SizeType>(profile_name, i));
	}
}

const Value& CConfig::getActiveProfile() {
	return document[active_profile];
}

bool CConfig::isSidColorAvail(const string& sid, const string& airport) {
	if (getActiveProfile().HasMember("maps"))
	{
		if (getActiveProfile()["maps"].HasMember(airport.c_str()))
		{
			if (getActiveProfile()["maps"][airport.c_str()].HasMember("sids") && getActiveProfile()["maps"][airport.c_str()]["sids"].IsArray())
			{
				const Value& SIDs = getActiveProfile()["maps"][airport.c_str()]["sids"];
				for (SizeType i = 0; i < SIDs.Size(); i++)
				{
					const Value& SIDNames = SIDs[i]["names"];
					for (SizeType s = 0; s < SIDNames.Size(); s++) {
						string currentsid = SIDNames[s].GetString();
						std::transform(currentsid.begin(), currentsid.end(), currentsid.begin(), ::toupper);
						if (startsWith(sid.c_str(), currentsid.c_str()))
						{
							return true;
						}
					}
				}
			}
		}
	}
	return false;
}

Gdiplus::Color CConfig::getSidColor(const string& sid, const string& airport)
{
	if (getActiveProfile().HasMember("maps"))
	{
		if (getActiveProfile()["maps"].HasMember(airport.c_str()))
		{
			if (getActiveProfile()["maps"][airport.c_str()].HasMember("sids") && getActiveProfile()["maps"][airport.c_str()]["sids"].IsArray())
			{
				const Value& SIDs = getActiveProfile()["maps"][airport.c_str()]["sids"];
				for (SizeType i = 0; i < SIDs.Size(); i++)
				{
					const Value& SIDNames = SIDs[i]["names"];
					for (SizeType s = 0; s < SIDNames.Size(); s++) {
						string currentsid = SIDNames[s].GetString();
						std::transform(currentsid.begin(), currentsid.end(), currentsid.begin(), ::toupper);
						if (startsWith(sid.c_str(), currentsid.c_str()))
						{
							return getConfigColor(SIDs[i]["color"]);
						}
					}
				}
			}
		}
	}
	return { 0, 0, 0 };
}

Gdiplus::Color CConfig::getConfigColor(const Value& config_path) {
	const int r = config_path["r"].GetInt();
	const int g = config_path["g"].GetInt();
	const int b = config_path["b"].GetInt();
	int a = 255;
	if (config_path.HasMember("a"))
		a = config_path["a"].GetInt();

	return Gdiplus::Color(a, r, g, b);
}

COLORREF CConfig::getConfigColorRef(const Value& config_path) {
	const int r = config_path["r"].GetInt();
	const int g = config_path["g"].GetInt();
	const int b = config_path["b"].GetInt();

	const COLORREF Color(RGB(r, g, b));
	return Color;
}

const Value& CConfig::getAirportMapIfAny(const string& airport) {
	if (getActiveProfile().HasMember("maps")) {
		const Value& map_data = getActiveProfile()["maps"];
		if (map_data.HasMember(airport.c_str())) {
			const Value& airport_map = map_data[airport.c_str()];
			return airport_map;
		}
	}
	return getActiveProfile();
}

bool CConfig::isAirportMapAvail(const string& airport) {
	if (getActiveProfile().HasMember("maps")) {
		if (getActiveProfile()["maps"].HasMember(airport.c_str())) {
			return true;
		}
	}
	return false;
}

bool CConfig::isCustomCursorUsed() {
	if (getActiveProfile().HasMember("cursor")) {
		if (strcmp(getActiveProfile()["cursor"].GetString(), "Default") == 0) {
			return false;
		}
	}
	return true; // by default use custom one so we don't break compatibility for old json settings that don't have the entry
}

auto CConfig::isCustomRunwayAvail(const string& airport, const string& name1, const string& name2) -> bool
{
	if (!getActiveProfile().HasMember("maps"))
		return false;
	if (!getActiveProfile()["maps"].HasMember(airport.c_str()))
		return false;
	if (!getActiveProfile()["maps"][airport.c_str()].HasMember("runways") || !getActiveProfile()["maps"][airport.c_str()]["runways"].IsArray())
		return false;

	const Value& Runways = getActiveProfile()["maps"][airport.c_str()]["runways"];
	for (SizeType i = 0; i < Runways.Size(); i++) {
		if (startsWith(name1.c_str(), Runways[i]["runway_name"].GetString()) ||
			startsWith(name2.c_str(), Runways[i]["runway_name"].GetString())) {
			return true;
		}
	}
	return false;
}

vector<string> CConfig::getAllProfiles() {
	vector<string> toR;
	for (auto it = profiles.begin(); it != profiles.end(); ++it)
	{
		toR.push_back(it->first);
	}
	return toR;
}

CConfig::~CConfig() = default;
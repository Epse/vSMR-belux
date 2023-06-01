#pragma once
#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <map>
#include <vector>
#include <Gdiplus.h>
#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "Constant.hpp"

using namespace std;
using namespace rapidjson;

class CConfig
{
public:
	CConfig(const string& configPath);
	virtual ~CConfig();

	const Value& getActiveProfile();
	bool isSidColorAvail(const string& sid, const string& airport);
	Gdiplus::Color getSidColor(const string& sid, const string& airport);
	const Value& getAirportMapIfAny(const string& airport);
	bool isAirportMapAvail(const string& airport);
	bool isCustomRunwayAvail(const string& airport, const string& name1, const string& name2);
	bool isCustomCursorUsed();

	static Gdiplus::Color getConfigColor(const Value& config_path);
	COLORREF getConfigColorRef(const Value& config_path);

	vector<string> getAllProfiles();

	inline int isItActiveProfile(const string& toTest) {
		if (active_profile == profiles[toTest])
			return 1;
		return 0;
	};

	inline void setActiveProfile(const string& newProfile) {
		active_profile = profiles[newProfile];
	};

	inline string getActiveProfileName() {
		string name;
		for (auto it = profiles.begin(); it != profiles.end(); ++it)
		{
			if (it->second == active_profile) {
				name = it->first;
				break;
			}
		}
		return name;
	};

	Document document;

protected:
	string config_path;
	rapidjson::SizeType active_profile;
	map<string, rapidjson::SizeType> profiles;

	void loadConfig();
};

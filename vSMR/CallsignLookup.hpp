#pragma once
#include <fstream>
#include <string>
#include <sstream>
#include <map>
#include <vector>

using namespace std;

class CCallsignLookup
{
private:
	std::map<string, string> callsigns;


public:

	void readFile(const string& fileName);
	string getCallsign(const string& airlineCode);
};
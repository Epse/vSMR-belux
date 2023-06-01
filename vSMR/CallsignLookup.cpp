#include "stdafx.h"
#include "CallsignLookup.hpp"

//
// CCallsignLookup Class by Even Rognlien, used with permission
//

void CCallsignLookup::readFile(const string& fileName)
{

	ifstream myfile;

	myfile.open(fileName);

	if (myfile) {
		string line;

		while (getline(myfile, line)) {
			istringstream iss(line);
			vector<string> tokens;
			string token;

			while (std::getline(iss, token, '\t'))
				tokens.push_back(token);

			if (tokens.size() >= 3) {
				callsigns[tokens.front()] = tokens.at(2);
			}
		}
	}

	myfile.close();
}

string CCallsignLookup::getCallsign(const string& airlineCode) {

	if (callsigns.find(airlineCode) == callsigns.end())
		return "";

	return callsigns.find(airlineCode)->second;
}

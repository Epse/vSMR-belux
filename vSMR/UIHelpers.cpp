#include "stdafx.h"
#include "UIHelpers.hpp"

string UIHelper::getEnumString(TagTypes type) {
	if (type == TagTypes::Departure)
		return "departure";
	if (type == TagTypes::Arrival)
		return "arrival";
	if (type == TagTypes::Uncorrelated)
		return "uncorrelated";
	return "airborne";
}

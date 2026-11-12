#pragma once
#include <EuroScopePlugIn.h>

inline bool is_mlat(const EuroScopePlugIn::CRadarTarget* rt)
{
	return rt->GetPosition().GetTransponderC();
}

inline bool is_airborne(const EuroScopePlugIn::CRadarTarget* rt)
{
	return rt->GetPosition().GetReportedGS() > 50;
}
inline bool is_airborne(const EuroScopePlugIn::CRadarTargetPositionData* rt)
{
	return rt->GetReportedGS() > 50;
}

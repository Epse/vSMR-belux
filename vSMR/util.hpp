#pragma once
#include <EuroScopePlugIn.h>

inline bool is_mlat(const EuroScopePlugIn::CRadarTarget* rt)
{
	return rt->GetPosition().GetTransponderC();
}

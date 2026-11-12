#pragma once

struct Layers
{
	bool vfr_fixes : 1;
	bool ifr_fixes : 1;
	bool ctr : 1;
	bool tma : 1;
	bool ranges_1 : 1;
	bool ranges_2 : 1;

	constexpr static Layers init()
	{
		return {.vfr_fixes = true, .ifr_fixes = true, .ctr = true , .tma = true, .ranges_1 = true, .ranges_2 = true};
	}
};

#pragma once

struct Filters
{
	bool show_free : 1;
	bool show_nonmine : 1;
	bool show_on_blocks : 1;
	bool show_nsts : 1;
};

inline Filters new_filters()
{
	return Filters{
		true,
		true,
		true,
		true,
	};
}

inline char char_from_filters(const Filters &filters)
{
	char converted = 0;
	if (filters.show_free)
	{
		converted |= 1;
	}
	if (filters.show_nonmine)
	{
		converted |= (1 << 1);
	}
	if (filters.show_on_blocks)
	{
		converted |= (1 << 2);
	}
	if (filters.show_nsts)
	{
		converted |= (1 << 3);
	}
	return converted;
}

inline Filters filters_from_char(const char filters)
{
	return Filters{
		(filters & 1) > 0,
		(filters & (1 << 1)) > 0,
		(filters & (1 << 2)) > 0,
		(filters & (1 << 3)) > 0,
	};
}

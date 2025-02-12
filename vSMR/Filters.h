#pragma once

struct Filters
{
	bool show_free : 1;
	bool show_nonmine : 1;
	bool show_on_blocks : 1;
	bool show_nsts : 1;
	bool show_stup : 1;
};

inline Filters new_filters()
{
	return Filters{
		true,
		true,
		true,
		true,
		true,
	};
}

inline char char_from_filters(const Filters &filters)
{
	// Initialize the top 4 bits to true
	// Those were unused and thus 0 before introduction of show_stup,
	// which would make show_stup default to false on existing installs.
	// Thus, we set bits if the field is set for the low bits,
	// and unset them for the high bits.
	unsigned char converted = 0xf0;
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
	if (filters.show_stup)
	{
		converted &= ~(1 << 4);
	}
	return static_cast<char>(converted);
}

inline Filters filters_from_char(const char filters)
{
	// Same as above, low bits: 1 means true, high bits are flipped.
	return Filters{
		(filters & 1) > 0,
		(filters & (1 << 1)) > 0,
		(filters & (1 << 2)) > 0,
		(filters & (1 << 3)) > 0,
		(filters & (1 << 4)) == 0,
	};
}

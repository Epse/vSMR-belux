#pragma once
#include <bit>

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

inline std::string str_from_filters(const Filters &filters)
{
	// Make it a "readable" number, to avoid odd characters formed by the bitfield breaking encoding
	const auto num = std::bit_cast<uint8_t>(filters);
	return std::to_string(num);
}

inline Filters filters_from_str(const std::string &filters)
{
	try
	{
		const uint8_t num = static_cast<uint8_t>(std::stoul(filters));
		return std::bit_cast<Filters>(num);
	} catch (...)
	{
		return new_filters();
	}
}

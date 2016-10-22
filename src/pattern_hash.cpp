#include <cmath>
#include <emilib/irange.hpp>
#include "pattern_hash.hpp"

PatternHash hash_from_pattern(const Pattern& pattern, size_t palette_size)
{
	CHECK_LT_F(std::pow((double)palette_size, (double)pattern.size()),
	           std::pow(2.0, sizeof(PatternHash) * 8),
	           "Too large palette (it is %lu) or too large pattern size (it's %.0f)",
	           palette_size, std::sqrt(pattern.size()));
	PatternHash result = 0;
	size_t power = 1;
	for (const auto i : irange(pattern.size()))
	{
		result += pattern[pattern.size() - 1 - i] * power;
		power *= palette_size;
	}
	return result;
}


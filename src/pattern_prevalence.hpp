#ifndef WFC_PATTERN_PREVALENCE_HPP
#define WFC_PATTERN_PREVALENCE_HPP

#include "constants.hpp"
#include "paletted_image.hpp"

// n = side of the pattern, e.g. 3.
PatternPrevalence extract_patterns(
	const PalettedImage& sample,
    int n,
    bool periodic_in,
    size_t symmetry,
	PatternHash* out_lowest_pattern
);


#endif


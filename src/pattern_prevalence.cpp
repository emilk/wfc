#include "pattern_prevalence.hpp"
#include "constants.hpp"
#include "pattern_hash.hpp"
#include "pattern.hpp"
#include <loguru.hpp>

PatternPrevalence extract_patterns(
	const PalettedImage& sample,
    int n,
    bool periodic_in,
    size_t symmetry,
	PatternHash* out_lowest_pattern
) {
	CHECK_LE_F(n, sample.width);
	CHECK_LE_F(n, sample.height);

	const auto pattern_from_sample = [&](size_t x, size_t y) {
		return make_pattern(n, [&](size_t dx, size_t dy){ return sample.at_wrapped(x + dx, y + dy); });
	};
	const auto rotate  = [&](const Pattern& p){ return make_pattern(n, [&](size_t x, size_t y){ return p[n - 1 - y + x * n]; }); };
	const auto reflect = [&](const Pattern& p){ return make_pattern(n, [&](size_t x, size_t y){ return p[n - 1 - x + y * n]; }); };

	PatternPrevalence patterns;

	for (size_t y : irange(periodic_in ? sample.height : sample.height - n + 1)) {
		for (size_t x : irange(periodic_in ? sample.width : sample.width - n + 1)) {
			std::array<Pattern, 8> ps;
			ps[0] = pattern_from_sample(x, y);
			ps[1] = reflect(ps[0]);
			ps[2] = rotate(ps[0]);
			ps[3] = reflect(ps[2]);
			ps[4] = rotate(ps[2]);
			ps[5] = reflect(ps[4]);
			ps[6] = rotate(ps[4]);
			ps[7] = reflect(ps[6]);

			for (int k = 0; k < symmetry; ++k) {
				auto hash = hash_from_pattern(ps[k], sample.palette.size());
				patterns[hash] += 1;
				if (out_lowest_pattern && y == sample.height - 1) {
					*out_lowest_pattern = hash;
				}
			}
		}
	}

	return patterns;
}


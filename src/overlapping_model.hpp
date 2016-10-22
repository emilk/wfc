#ifndef WFC_OVERLAPPING_MODEL_HPP
#define WFC_OVERLAPPING_MODEL_HPP

#include <vector>
#include <cmath>
#include "model.hpp"
#include "constants.hpp"
#include "array3d.hpp"

Pattern pattern_from_hash(const PatternHash hash, int n, size_t palette_size);

class OverlappingModel : public Model
{
public:
	OverlappingModel(
		const PatternPrevalence& hashed_patterns,
		const Palette&           palette,
		int                      n,
		bool                     periodic_out,
		size_t                   width,
		size_t                   height,
		PatternHash              foundation_pattern);

	bool propagate(Output* output) const override;

	bool on_boundary(int x, int y) const override
	{
		return !_periodic_out && (x + _n > _width || y + _n > _height);
	}

	Image image(const Output& output) const override;

	Graphics graphics(const Output& output) const;

private:
	int                       _n;
	// num_patterns X (2 * n - 1) X (2 * n - 1) X ???
	// list of other pattern indices that agree on this x/y offset (?)
	Array3D<std::vector<PatternIndex>> _propagator;
	std::vector<Pattern>               _patterns;
	Palette                            _palette;
};

#endif


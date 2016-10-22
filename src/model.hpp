#ifndef WFC_MODEL_HPP
#define WFC_MODEL_HPP

#include <vector>
#include "constants.hpp"
#include "output.hpp"


class Model
{
public:
	size_t              _width;      // Of output image.
	size_t              _height;     // Of output image.
	size_t              _num_patterns;
	bool                _periodic_out;
	size_t              _foundation = kInvalidIndex; // Index of pattern which is at the base, or kInvalidIndex

	// The weight of each pattern (e.g. how often that pattern occurs in the sample image).
	std::vector<double> _pattern_weight; // num_patterns

	virtual bool propagate(Output* output) const = 0;
	virtual bool on_boundary(int x, int y) const = 0;
	virtual Image image(const Output& output) const = 0;
};

#endif


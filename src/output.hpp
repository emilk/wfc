#ifndef WFC_OUTPUT_HPP
#define WFC_OUTPUT_HPP

#include "array2d.hpp"
#include "array3d.hpp"

// What actually changes
struct Output
{
	// _width X _height X num_patterns
	// _wave.get(x, y, t) == is the pattern t possible at x, y?
	// Starts off true everywhere.
	Array3D<Bool> _wave;
	Array2D<Bool> _changes; // _width X _height. Starts off false everywhere.

};

struct Model;
Output create_output(const Model& model);

#endif


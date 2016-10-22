#ifndef WFC_PALETTED_IMAGE_HPP
#define WFC_PALETTED_IMAGE_HPP

#include <vector>
#include "constants.hpp"

struct PalettedImage
{
	size_t                  width, height;
	std::vector<ColorIndex> data; // width * height
	Palette                 palette;

	ColorIndex at_wrapped(size_t x, size_t y) const
	{
		return data[width * (y % height) + (x % width)];
	}
};

PalettedImage load_paletted_image(const std::string& path);

#endif


#ifndef WFC_IMAGE_HPP
#define WFC_IMAGE_HPP

#include "constants.hpp"

Image upsample(const Image& image);
Image image_from_graphics(const Graphics& graphics, const Palette& palette);

#endif


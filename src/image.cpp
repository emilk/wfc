#include "image.hpp"

Image upsample(const Image& image)
{
	Image result(image.width() * kUpscale, image.height() * kUpscale, {});
	for (const auto y : irange(result.height())) {
		for (const auto x : irange(result.width())) {
			result.set(x, y, image.get(x / kUpscale, y / kUpscale));
		}
	}
	return result;
}


Image image_from_graphics(const Graphics& graphics, const Palette& palette)
{
	Image result(graphics.width(), graphics.height(), {0, 0, 0, 0});

	for (const auto y : irange(graphics.height())) {
		for (const auto x : irange(graphics.width())) {
			const auto& tile_constributors = graphics.ref(x, y);
			if (tile_constributors.empty()) {
				result.set(x, y, {0, 0, 0, 255});
			} else if (tile_constributors.size() == 1) {
				result.set(x, y, palette[tile_constributors[0]]);
			} else {
				size_t r = 0;
				size_t g = 0;
				size_t b = 0;
				size_t a = 0;
				for (const auto tile : tile_constributors) {
					r += palette[tile].r;
					g += palette[tile].g;
					b += palette[tile].b;
					a += palette[tile].a;
				}
				r /= tile_constributors.size();
				g /= tile_constributors.size();
				b /= tile_constributors.size();
				a /= tile_constributors.size();
				result.set(x, y, {(uint8_t)r, (uint8_t)g, (uint8_t)b, (uint8_t)a});
			}
		}
	}

	return result;
}

Image scroll_diagonally(const Image& image)
{
	const auto width = image.width();
	const auto height = image.height();
	Image result(width, height);
	for (const auto y : irange(height)) {
		for (const auto x : irange(width)) {
			result.set(x, y, image.get((x + 1) % width, (y + 1) % height));
		}
	}
	return result;
}



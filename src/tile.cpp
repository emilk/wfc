#include <loguru.hpp>
#include "tile.hpp"

Tile rotate(const Tile& in_tile, const size_t tile_size)
{
	CHECK_EQ_F(in_tile.size(), tile_size * tile_size);
	Tile out_tile;
	for (size_t y : irange(tile_size)) {
		for (size_t x : irange(tile_size)) {
			out_tile.push_back(in_tile[tile_size - 1 - y + x * tile_size]);
		}
	}
	return out_tile;
}


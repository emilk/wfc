#ifndef WFC_TILE_MODEL_HPP
#define WFC_TILE_MODEL_HPP

#include <vector>
#include <configuru.hpp>
#include "model.hpp"
#include "output.hpp"
#include "array3d.hpp"
#include "rgba.hpp"
#include "constants.hpp"

class TileModel : public Model
{
public:
	TileModel(const configuru::Config& config, std::string subset_name, int width, int height, bool periodic, const TileLoader& tile_loader);

	bool propagate(Output* output) const override;

	bool on_boundary(int x, int y) const override
	{
		return false;
	}

	Image image(const Output& output) const override;

private:
	Array3D<Bool>                  _propagator; // 4 X _num_patterns X _num_patterns
	std::vector<std::vector<RGBA>> _tiles;
	size_t                         _tile_size;
};

#endif


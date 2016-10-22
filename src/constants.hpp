#ifndef WFC_CONSTANTS_HPP
#define WFC_CONSTANTS_HPP

#include <unordered_map>
#include <vector>
#include <functional>
#include <string>
#include <emilib/irange.hpp>
#include "array2d.hpp"
#include "rgba.hpp"

const auto kUsage = R"(
wfc.bin [-h/--help] [--gif] [job=samples.cfg, ...]
	-h/--help   Print this help
	--gif       Export GIF images of the process
	file        Jobs to run
)";

using emilib::irange;

using Bool              = uint8_t; // To avoid problems with vector<bool>
using ColorIndex        = uint8_t; // tile index or color index. If you have more than 255, don't.
using Palette           = std::vector<RGBA>;
using Pattern           = std::vector<ColorIndex>;
using PatternHash       = uint64_t; // Another representation of a Pattern.
using PatternPrevalence = std::unordered_map<PatternHash, size_t>;
using RandomDouble      = std::function<double()>;
using PatternIndex      = uint16_t;
using Graphics          = Array2D<std::vector<ColorIndex>>;
using Image             = Array2D<RGBA>;
using Tile              = std::vector<RGBA>;
using TileLoader        = std::function<Tile(const std::string& tile_name)>;

const auto kInvalidIndex = static_cast<size_t>(-1);
const auto kInvalidHash = static_cast<PatternHash>(-1);

const bool   kGifSeparatePalette  = true;
const size_t kGifInterval         =  16; // Save an image every X iterations
const int    kGifDelayCentiSec    =   1;
const int    kGifEndPauseCentiSec = 200;
const size_t kUpscale             =   4; // Upscale images before saving

const size_t MAX_COLORS = 1 << (sizeof(ColorIndex) * 8);


#endif


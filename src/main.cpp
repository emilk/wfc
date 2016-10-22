#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <memory>
#include <numeric>
#include <random>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <configuru.hpp>
#include <emilib/strprintf.hpp>
#include <loguru.hpp>
#include <stb_image.h>
#include <stb_image_write.h>

#define JO_GIF_HEADER_FILE_ONLY
#include <jo_gif.cpp>

#include "constants.hpp"
#include "array2d.hpp"
#include "array3d.hpp"
#include "rgba.hpp"
#include "options.hpp"
#include "result.hpp"
#include "paletted_image.hpp"
#include "output.hpp"
#include "model.hpp"
#include "overlapping_model.hpp"
#include "tile_model.hpp"
#include "image.hpp"


const char* result2str(const Result result)
{
	return result == Result::kSuccess ? "success"
	     : result == Result::kFail    ? "fail"
	     : "unfinished";
}




double calc_sum(const std::vector<double>& a)
{
	return std::accumulate(a.begin(), a.end(), 0.0);
}

// Pick a random index weighted by a
size_t spin_the_bottle(const std::vector<double>& a, double between_zero_and_one)
{
	double sum = calc_sum(a);

	if (sum == 0.0) {
		return std::floor(between_zero_and_one * a.size());
	}

	double between_zero_and_sum = between_zero_and_one * sum;

	double accumulated = 0;

	for (auto i : irange(a.size())) {
		accumulated += a[i];
		if (between_zero_and_sum <= accumulated) {
			return i;
		}
	}

	return 0;
}

PatternHash hash_from_pattern(const Pattern& pattern, size_t palette_size)
{
	CHECK_LT_F(std::pow((double)palette_size, (double)pattern.size()),
	           std::pow(2.0, sizeof(PatternHash) * 8),
	           "Too large palette (it is %lu) or too large pattern size (it's %.0f)",
	           palette_size, std::sqrt(pattern.size()));
	PatternHash result = 0;
	size_t power = 1;
	for (const auto i : irange(pattern.size()))
	{
		result += pattern[pattern.size() - 1 - i] * power;
		power *= palette_size;
	}
	return result;
}


template<typename Fun>
Pattern make_pattern(int n, const Fun& fun)
{
	Pattern result(n * n);
	for (auto dy : irange(n)) {
		for (auto dx : irange(n)) {
			result[dy * n + dx] = fun(dx, dy);
		}
	}
	return result;
};


Image OverlappingModel::image(const Output& output) const
{
	return upsample(image_from_graphics(graphics(output), _palette));
}

// ----------------------------------------------------------------------------

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

TileModel::TileModel(const configuru::Config& config, std::string subset_name, int width, int height, bool periodic_out, const TileLoader& tile_loader)
{
	_width        = width;
	_height       = height;
	_periodic_out = periodic_out;

	_tile_size        = config.get_or("tile_size", 16);
	const bool unique = config.get_or("unique",    false);

	std::unordered_set<std::string> subset;
	if (subset_name != "") {
		for (const auto& tile_name : config["subsets"][subset_name].as_array()) {
			subset.insert(tile_name.as_string());
		}
	}

	std::vector<std::array<int,     8>>  action;
	std::unordered_map<std::string, size_t> first_occurrence;

	for (const auto& tile : config["tiles"].as_array()) {
		const std::string tile_name = tile["name"].as_string();
		if (!subset.empty() && subset.count(tile_name) == 0) { continue; }

		std::function<int(int)> a, b;
		int cardinality;

		std::string sym = tile.get_or("symmetry", "X");
		if (sym == "L") {
			cardinality = 4;
			a = [](int i){ return (i + 1) % 4; };
			b = [](int i){ return i % 2 == 0 ? i + 1 : i - 1; };
		} else if (sym == "T") {
			cardinality = 4;
			a = [](int i){ return (i + 1) % 4; };
			b = [](int i){ return i % 2 == 0 ? i : 4 - i; };
		} else if (sym == "I") {
			cardinality = 2;
			a = [](int i){ return 1 - i; };
			b = [](int i){ return i; };
		} else if (sym == "\\") {
			cardinality = 2;
			a = [](int i){ return 1 - i; };
			b = [](int i){ return 1 - i; };
		} else if (sym == "X") {
			cardinality = 1;
			a = [](int i){ return i; };
			b = [](int i){ return i; };
		} else {
			ABORT_F("Unknown symmetry '%s'", sym.c_str());
		}

		const size_t num_patterns_so_far = action.size();
		first_occurrence[tile_name] = num_patterns_so_far;

		for (int t = 0; t < cardinality; ++t) {
			std::array<int, 8> map;

			map[0] = t;
			map[1] = a(t);
			map[2] = a(a(t));
			map[3] = a(a(a(t)));
			map[4] = b(t);
			map[5] = b(a(t));
			map[6] = b(a(a(t)));
			map[7] = b(a(a(a(t))));

			for (int s = 0; s < 8; ++s) {
				map[s] += num_patterns_so_far;
			}

			action.push_back(map);
		}

		if (unique) {
			for (int t = 0; t < cardinality; ++t) {
				const Tile bitmap = tile_loader(emilib::strprintf("%s %d", tile_name.c_str(), t));
				CHECK_EQ_F(bitmap.size(), _tile_size * _tile_size);
				_tiles.push_back(bitmap);
			}
		} else {
			const Tile bitmap = tile_loader(emilib::strprintf("%s", tile_name.c_str()));
			CHECK_EQ_F(bitmap.size(), _tile_size * _tile_size);
			_tiles.push_back(bitmap);
			for (int t = 1; t < cardinality; ++t) {
				_tiles.push_back(rotate(_tiles[num_patterns_so_far + t - 1], _tile_size));
			}
		}

		for (int t = 0; t < cardinality; ++t) {
			_pattern_weight.push_back(tile.get_or("weight", 1.0));
		}
	}

	_num_patterns = action.size();

	_propagator = Array3D<Bool>(4, _num_patterns, _num_patterns, false);

	for (const auto& neighbor : config["neighbors"].as_array()) {
		const auto left  = neighbor["left"];
		const auto right = neighbor["right"];
		CHECK_EQ_F(left.array_size(),  2u);
		CHECK_EQ_F(right.array_size(), 2u);

		const auto left_tile_name = left[0].as_string();
		const auto right_tile_name = right[0].as_string();

		if (!subset.empty() && (subset.count(left_tile_name) == 0 || subset.count(right_tile_name) == 0)) { continue; }

		int L = action[first_occurrence[left_tile_name]][left[1].get<int>()];
		int R = action[first_occurrence[right_tile_name]][right[1].get<int>()];
		int D = action[L][1];
		int U = action[R][1];

		_propagator.set(0, L,            R,            true);
		_propagator.set(0, action[L][6], action[R][6], true);
		_propagator.set(0, action[R][4], action[L][4], true);
		_propagator.set(0, action[R][2], action[L][2], true);

		_propagator.set(1, D,            U,            true);
		_propagator.set(1, action[U][6], action[D][6], true);
		_propagator.set(1, action[D][4], action[U][4], true);
		_propagator.set(1, action[U][2], action[D][2], true);
	}

	for (int t1 = 0; t1 < _num_patterns; ++t1) {
		for (int t2 = 0; t2 < _num_patterns; ++t2) {
			_propagator.set(2, t1, t2, _propagator.get(0, t2, t1));
			_propagator.set(3, t1, t2, _propagator.get(1, t2, t1));
		}
	}
}

PalettedImage load_paletted_image(const std::string& path)
{
	ERROR_CONTEXT("loading sample image", path.c_str());
	int width, height, comp;
	RGBA* rgba = reinterpret_cast<RGBA*>(stbi_load(path.c_str(), &width, &height, &comp, 4));
	CHECK_NOTNULL_F(rgba);
	const auto num_pixels = width * height;

	// Fix issues with stbi_load:
	if (comp == 1) {
		// input was greyscale - set alpha:
		for (auto& pixel : emilib::it_range(rgba, rgba + num_pixels)) {
			pixel.a = pixel.r;
		}
	} else {
		if (comp == 3) {
			for (auto& pixel : emilib::it_range(rgba, rgba + num_pixels)) {
				pixel.a = 255;
			}
		}
		for (auto& pixel : emilib::it_range(rgba, rgba + num_pixels)) {
			if (pixel.a == 0) {
				pixel = RGBA{0,0,0,0};
			}
		}
	}

	std::vector<RGBA> palette;
	std::vector<ColorIndex> data;

	for (const auto pixel_idx : irange(num_pixels)) {
		const RGBA color = rgba[pixel_idx];
		const auto color_idx = std::find(palette.begin(), palette.end(), color) - palette.begin();
		if (color_idx == palette.size()) {
			CHECK_LT_F(palette.size(), MAX_COLORS, "Too many colors in image");
			palette.push_back(color);
		}
		data.push_back(color_idx);
	}

	stbi_image_free(rgba);

	return PalettedImage{
		static_cast<size_t>(width),
		static_cast<size_t>(height),
		data, palette
	};
}

// n = side of the pattern, e.g. 3.
PatternPrevalence extract_patterns(
	const PalettedImage& sample, int n, bool periodic_in, size_t symmetry,
	PatternHash* out_lowest_pattern)
{
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

Result find_lowest_entropy(const Model& model, const Output& output, RandomDouble& random_double,
                           int* argminx, int* argminy)
{
	// We actually calculate exp(entropy), i.e. the sum of the weights of the possible patterns

	double min = std::numeric_limits<double>::infinity();

	for (int x = 0; x < model._width; ++x) {
		for (int y = 0; y < model._height; ++y) {
			if (model.on_boundary(x, y)) { continue; }

			size_t num_superimposed = 0;
			double entropy = 0;

			for (int t = 0; t < model._num_patterns; ++t) {
				if (output._wave.get(x, y, t)) {
					num_superimposed += 1;
					entropy += model._pattern_weight[t];
				}
			}

			if (entropy == 0 || num_superimposed == 0) {
				return Result::kFail;
			}

			if (num_superimposed == 1) {
				continue; // Already frozen
			}

			// Add a tie-breaking bias:
			const double noise = 0.5 * random_double();
			entropy += noise;

			if (entropy < min) {
				min = entropy;
				*argminx = x;
				*argminy = y;
			}
		}
	}

	if (min == std::numeric_limits<double>::infinity()) {
		return Result::kSuccess;
	} else {
		return Result::kUnfinished;
	}
}

Result observe(const Model& model, Output* output, RandomDouble& random_double)
{
	int argminx, argminy;
	const auto result = find_lowest_entropy(model, *output, random_double, &argminx, &argminy);
	if (result != Result::kUnfinished) { return result; }

	std::vector<double> distribution(model._num_patterns);
	for (int t = 0; t < model._num_patterns; ++t) {
		distribution[t] = output->_wave.get(argminx, argminy, t) ? model._pattern_weight[t] : 0;
	}
	size_t r = spin_the_bottle(std::move(distribution), random_double());
	for (int t = 0; t < model._num_patterns; ++t) {
		output->_wave.set(argminx, argminy, t, t == r);
	}
	output->_changes.set(argminx, argminy, true);

	return Result::kUnfinished;
}

Output create_output(const Model& model)
{
	Output output;
	output._wave = Array3D<Bool>(model._width, model._height, model._num_patterns, true);
	output._changes = Array2D<Bool>(model._width, model._height, false);

	if (model._foundation != kInvalidIndex) {
		for (const auto x : irange(model._width)) {
			for (const auto t : irange(model._num_patterns)) {
				if (t != model._foundation) {
					output._wave.set(x, model._height - 1, t, false);
				}
			}
			output._changes.set(x, model._height - 1, true);

			for (const auto y : irange(model._height - 1)) {
				output._wave.set(x, y, model._foundation, false);
				output._changes.set(x, y, true);
			}

			while (model.propagate(&output));
		}
	}

	return output;
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

Result run(Output* output, const Model& model, size_t seed, size_t limit, jo_gif_t* gif_out)
{
	std::mt19937 gen(seed);
	std::uniform_real_distribution<double> dis(0.0, 1.0);
	RandomDouble random_double = [&]() { return dis(gen); };

	for (size_t l = 0; l < limit || limit == 0; ++l) {
		Result result = observe(model, output, random_double);

		if (gif_out && l % kGifInterval == 0) {
			const auto image = model.image(*output);
			jo_gif_frame(gif_out, (uint8_t*)image.data(), kGifDelayCentiSec, kGifSeparatePalette);
		}

		if (result != Result::kUnfinished) {
			if (gif_out) {
				// Pause on the last image:
				auto image = model.image(*output);
				jo_gif_frame(gif_out, (uint8_t*)image.data(), kGifEndPauseCentiSec, kGifSeparatePalette);

				if (model._periodic_out) {
					// Scroll the image diagonally:
					for (size_t i = 0; i < model._width; ++i) {
						image = scroll_diagonally(image);
						jo_gif_frame(gif_out, (uint8_t*)image.data(), kGifDelayCentiSec, kGifSeparatePalette);
					}
				}
			}

			LOG_F(INFO, "%s after %lu iterations", result2str(result), l);
			return result;
		}
		while (model.propagate(output));
	}

	LOG_F(INFO, "Unfinished after %lu iterations", limit);
	return Result::kUnfinished;
}

void run_and_write(const Options& options, const std::string& name, const configuru::Config& config, const Model& model)
{
	const size_t limit       = config.get_or("limit",       0);
	const size_t screenshots = config.get_or("screenshots", 2);

	for (const auto i : irange(screenshots)) {
		for (const auto attempt : irange(10)) {
			(void)attempt;
			int seed = rand();

			Output output = create_output(model);

			jo_gif_t gif;

			if (options.export_gif) {
				const auto initial_image = model.image(output);
				const auto gif_path = emilib::strprintf("output/%s_%lu.gif", name.c_str(), i);
				const int gif_palette_size = 255; // TODO
				gif = jo_gif_start(gif_path.c_str(), initial_image.width(), initial_image.height(), 0, gif_palette_size);
			}

			const auto result = run(&output, model, seed, limit, options.export_gif ? &gif : nullptr);

			if (options.export_gif) {
				jo_gif_end(&gif);
			}

			if (result == Result::kSuccess) {
				const auto image = model.image(output);
				const auto out_path = emilib::strprintf("output/%s_%lu.png", name.c_str(), i);
				CHECK_F(stbi_write_png(out_path.c_str(), image.width(), image.height(), 4, image.data(), 0) != 0,
				        "Failed to write image to %s", out_path.c_str());
				break;
			}
		}
	}
}

std::unique_ptr<Model> make_overlapping(const std::string& image_dir, const configuru::Config& config)
{
	const auto image_filename = config["image"].as_string();
	const auto in_path = image_dir + image_filename;

	const int    n              = config.get_or("n",             3);
	const size_t out_width      = config.get_or("width",        48);
	const size_t out_height     = config.get_or("height",       48);
	const size_t symmetry       = config.get_or("symmetry",      8);
	const bool   periodic_out   = config.get_or("periodic_out", true);
	const bool   periodic_in    = config.get_or("periodic_in",  true);
	const auto   has_foundation = config.get_or("foundation",   false);

	const auto sample_image = load_paletted_image(in_path.c_str());
	LOG_F(INFO, "palette size: %lu", sample_image.palette.size());
	PatternHash foundation = kInvalidHash;
	const auto hashed_patterns = extract_patterns(sample_image, n, periodic_in, symmetry, has_foundation ? &foundation : nullptr);
	LOG_F(INFO, "Found %lu unique patterns in sample image", hashed_patterns.size());

	return std::unique_ptr<Model>{
		new OverlappingModel{hashed_patterns, sample_image.palette, n, periodic_out, out_width, out_height, foundation}
	};
}

std::unique_ptr<Model> make_tiled(const std::string& image_dir, const configuru::Config& config)
{
	const std::string subdir     = config["subdir"].as_string();
	const size_t      out_width  = config.get_or("width",    48);
	const size_t      out_height = config.get_or("height",   48);
	const std::string subset     = config.get_or("subset",   std::string());
	const bool        periodic   = config.get_or("periodic", false);

	const TileLoader tile_loader = [&](const std::string& tile_name) -> Tile
	{
		const std::string path = emilib::strprintf("%s%s/%s.bmp", image_dir.c_str(), subdir.c_str(), tile_name.c_str());
		int width, height, comp;
		RGBA* rgba = reinterpret_cast<RGBA*>(stbi_load(path.c_str(), &width, &height, &comp, 4));
		CHECK_NOTNULL_F(rgba);
		const auto num_pixels = width * height;
		Tile tile(rgba, rgba + num_pixels);
		stbi_image_free(rgba);
		return tile;
	};

	const auto root_dir = image_dir + subdir + "/";
	const auto tile_config = configuru::parse_file(root_dir + "data.cfg", configuru::CFG);
	return std::unique_ptr<Model>{
		new TileModel(tile_config, subset, out_width, out_height, periodic, tile_loader)
	};
}

void run_config_file(const Options& options, const std::string& path)
{
	LOG_F(INFO, "Running all samples in %s", path.c_str());
	const auto samples = configuru::parse_file(path, configuru::CFG);
	const auto image_dir = samples["image_dir"].as_string();

	if (samples.count("overlapping")) {
		for (const auto& p : samples["overlapping"].as_object()) {
			LOG_SCOPE_F(INFO, "%s", p.key().c_str());
			const auto model = make_overlapping(image_dir, p.value());
			run_and_write(options, p.key(), p.value(), *model);
			p.value().check_dangling();
		}
	}

	if (samples.count("tiled")) {
		for (const auto& p : samples["tiled"].as_object()) {
			LOG_SCOPE_F(INFO, "Tiled %s", p.key().c_str());
			const auto model = make_tiled(image_dir, p.value());
			run_and_write(options, p.key(), p.value(), *model);
		}
	}
}

int main(int argc, char* argv[])
{
	loguru::init(argc, argv);

	Options options;

	std::vector<std::string> files;

	for (int i = 1; i < argc; ++i) {
		if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
			printf(kUsage);
			exit(0);
		} else if (strcmp(argv[i], "--gif") == 0) {
			options.export_gif = true;
			LOG_F(INFO, "Enabled GIF exporting");
		} else {
			files.push_back(argv[i]);
		}
	}

	if (files.empty()) {
		files.push_back("samples.cfg");
	}

	for (const auto& file : files) {
		run_config_file(options, file);
	}
}

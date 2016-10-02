#if 0
	set -eu
	echo "Compiling..."
	rm -f *.bin
	gcc --std=c++14 -Wall -O2 -lstdc++ -lpthread -ldl -I libs -I libs/emilib main.cpp -o wfc.bin
	mkdir -p output
	./wfc.bin
	exit
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>
#include <random>
#include <unordered_map>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION 1
#include <stb_image.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION 1
#include <stb_image_write.h>

#define CONFIGURU_IMPLEMENTATION 1
#include <configuru.hpp>

#define LOGURU_IMPLEMENTATION 1
#include <loguru.hpp>

#include <emilib/irange.hpp>
#include <emilib/strprintf.cpp>

using emilib::irange;

struct RGBA
{
	uint8_t r, g, b, a;
};
static_assert(sizeof(RGBA) == 4, "");
bool operator==(RGBA x, RGBA y) { return x.r == y.r && x.g == y.g && x.b == y.b && x.a == y.a; }

using Bool              = uint8_t; // To avoid problems with vector<bool>
using ColorIndex        = uint8_t; // tile index or color index. If you have more than 255, don't.
using Palette           = std::vector<RGBA>;
using Pattern           = std::vector<ColorIndex>;
using PatternHash       = uint64_t; // Another representation of a Pattern.
using PatternPrevalence = std::unordered_map<PatternHash, size_t>;
using RandomDouble      = std::function<double()>;
using Int               = int;

enum class Result
{
	kSuccess,
	kFail,
	kUnfinished,
};

const char* result2str(const Result result)
{
	return result == Result::kSuccess ? "success"
	     : result == Result::kFail    ? "fail"
	     : "unfinished";
}

const size_t MAX_COLORS = 1 << (sizeof(ColorIndex) * 8);

template<typename T>
struct Array2D
{
public:
	size_t width, height;
	std::vector<T> data;

	Array2D() : width(0), height(0) {}
	Array2D(size_t w, size_t h, T value) : width(w), height(h), data(w * h, value) {}

	T& operator()(size_t x, size_t y)
	{
		CHECK_LT_F(x, width);
		CHECK_LT_F(y, height);
		return data[y * width + x];
	}

	const T& operator()(size_t x, size_t y) const
	{
		CHECK_LT_F(x, width);
		CHECK_LT_F(y, height);
		return data[y * width + x];
	}

	auto begin() const { return data.begin(); }
	auto end()   const { return data.end();   }
};

template<typename T>
struct Array3D
{
public:
	size_t width, height, depth;
	std::vector<T> data;

	Array3D() : width(0), height(0), depth(0) {}
	Array3D(size_t w, size_t h, size_t d, T value) : width(w), height(h), depth(d), data(w * h * d, value) {}

	T& operator()(size_t x, size_t y, size_t z)
	{
		CHECK_LT_F(x, width);
		CHECK_LT_F(y, height);
		CHECK_LT_F(z, depth);
		return data[z * width * height + y * width + x];
	}

	const T& operator()(size_t x, size_t y, size_t z) const
	{
		CHECK_LT_F(x, width);
		CHECK_LT_F(y, height);
		CHECK_LT_F(z, depth);
		return data[z * width * height + y * width + x];
	}
};

using Graphics = Array2D<std::vector<ColorIndex>>;

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

// What actually changes
struct Output
{
	Array3D<Bool> _wave;    // _width X _height X num_patterns
	Array2D<Bool> _changes; // _width X _height
};

class Model
{
public:
	size_t              _width;      // Of output image.
	size_t              _height;     // Of output image.
	size_t              _num_patterns;
	Int                 _n;

	bool                _periodic_out;

	std::vector<double> _stationary; // num_patterns


	virtual bool propagate(Output* output) const = 0;
	virtual bool on_boundary(int x, int y) const = 0;
	virtual Graphics graphics(const Output& output) const = 0;
};

// ----------------------------------------------------------------------------

class OverlappingModel : public Model
{
public:
	OverlappingModel(
		const PatternPrevalence& hashed_patterns,
		Int n, size_t palette_size,
		bool periodic_out,
		size_t width, size_t height, size_t foundation);

	bool propagate(Output* output) const override;

	bool on_boundary(int x, int y) const override
	{
		return !_periodic_out && (x + _n > _width || y + _n > _height);
	}

	Graphics graphics(const Output& output) const override;

private:
	// num_patterns X (2 * n - 1) X (2 * n - 1) X ???
	// list of other pattern indices that agree on this x/y offset (?)
	Array3D<std::vector<int>> _propagator;
	// int         _n;
	std::vector<Pattern>      _patterns;
	size_t                    _foundation;
};

// ----------------------------------------------------------------------------

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
	PatternHash result = 0;
	size_t power = 1;
	for (const auto v : pattern)
	{
		result += v * power;
		power *= palette_size;
	}
	return result;
}

Pattern pattern_from_hash(const PatternHash hash, Int n, size_t palette_size)
{
	size_t residue = hash;
	size_t power = std::pow(palette_size, n * n);
	Pattern result(n * n);

	for (size_t i = 0; i < result.size(); ++i)
	{
		power /= palette_size;
		size_t count = 0;

		while (residue >= power)
		{
			residue -= power;
			count++;
		}

		result[i] = static_cast<ColorIndex>(count);
	}

	return result;
}

template<typename Fun>
Pattern make_pattern(Int n, const Fun& fun)
{
	Pattern result(n * n);
	for (auto dy : irange(n)) {
		for (auto dx : irange(n)) {
			result[dy * n + dx] = fun(dx, dy);
		}
	}
	return result;
};

// ----------------------------------------------------------------------------

OverlappingModel::OverlappingModel(
	const PatternPrevalence& hashed_patterns,
	Int n, size_t palette_size,
	bool periodic_out,
	size_t width, size_t height, size_t foundation)
{
	_width        = width;
	_height       = height;
	_num_patterns = hashed_patterns.size();
	_periodic_out = periodic_out;
	_n            = n;
	_foundation   = (_num_patterns + foundation) % _num_patterns;

	for (const auto& it : hashed_patterns)
	{
		_patterns.push_back(pattern_from_hash(it.first, n, palette_size));
		_stationary.push_back(it.second);
	}

	const auto agrees = [&](const Pattern& p1, const Pattern& p2, int dx, int dy)
	{
		int xmin = dx < 0 ? 0 : dx, xmax = dx < 0 ? dx + n : n;
		int ymin = dy < 0 ? 0 : dy, ymax = dy < 0 ? dy + n : n;
		for (int y = ymin; y < ymax; ++y) {
			for (int x = xmin; x < xmax; ++x) {
				if (p1[x + n * y] != p2[x - dx + n * (y - dy)]) {
					return false;
				}
			}
		}
		return true;
	};

	_propagator = Array3D<std::vector<int>>(_num_patterns, 2 * n - 1, 2 * n - 1, {});

	for (auto t : irange(_num_patterns)) {
		for (auto x : irange<int>(2 * n - 1)) {
			for (auto y : irange<int>(2 * n - 1)) {
				auto& list = _propagator(t, x, y);
				for (auto t2 : irange(_num_patterns)) {
					if (agrees(_patterns[t], _patterns[t2], x - n + 1, y - n + 1)) {
						list.push_back(t2);
					}
				}
				list.shrink_to_fit();
			}
		}
	}
}

bool OverlappingModel::propagate(Output* output) const
{
	bool did_change = false;

	for (int x1 = 0; x1 < _width; ++x1) {
		for (int y1 = 0; y1 < _height; ++y1) {
			auto& changes = output->_changes(x1, y1);
			if (!changes) { continue; }
			changes = false;

			for (int dx = -_n + 1; dx < _n; ++dx) {
				for (int dy = -_n + 1; dy < _n; ++dy) {
					auto x2 = x1 + dx;
					auto y2 = y1 + dy;

					auto sx = x2;
					if      (sx <  0)      { sx += _width; }
					else if (sx >= _width) { sx -= _width; }

					auto sy = y2;
					if      (sy <  0)       { sy += _height; }
					else if (sy >= _height) { sy -= _height; }

					if (!_periodic_out && (sx + _n > _width || sy + _n > _height)) {
						continue;
					}

					for (int t2 = 0; t2 < _num_patterns; ++t2) {
						bool b = false;
						const auto& prop = _propagator(t2, _n - 1 - dx, _n - 1 - dy);
						for (int i1 = 0; i1 < prop.size() && !b; ++i1) {
							b = output->_wave(x1, y1, prop[i1]);
						}

						if (output->_wave(sx, sy, t2) && !b) {
							output->_changes(sx, sy) = true;
							did_change = true;
							output->_wave(sx, sy, t2) = false;
						}
					}
				}
			}
		}
	}

	return did_change;
}

Graphics OverlappingModel::graphics(const Output& output) const
{
	Graphics result(_width, _height, {});
	for (const auto y : irange(_height)) {
		for (const auto x : irange(_width)) {
			auto& tile_constributors = result(x, y);

			for (int dy = 0; dy < _n; ++dy) {
				for (int dx = 0; dx < _n; ++dx) {
					int sx = x - dx;
					if (sx < 0) sx += _width;

					int sy = y - dy;
					if (sy < 0) sy += _height;

					if (on_boundary(sx, sy)) { continue; }

					for (int t = 0; t < _num_patterns; ++t) {
						if (output._wave(sx, sy, t)) {
							tile_constributors.push_back(_patterns[t][dx + dy * _n]);
						}
					}
				}
			}
		}
	}
	return result;
}

// ----------------------------------------------------------------------------

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

std::vector<RGBA> rgba_from_graphics(const Graphics& graphics, const Palette& palette)
{
	std::vector<RGBA> result;
	result.reserve(graphics.width * graphics.height);
	for (const auto tile_constributors : graphics) {
		if (tile_constributors.empty()) {
			result.push_back({0, 0, 0, 255});
		} else if (tile_constributors.size() == 1) {
			result.push_back(palette[tile_constributors[0]]);
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
			result.push_back({(uint8_t)r, (uint8_t)g, (uint8_t)b, (uint8_t)a});
		}
	}

	return result;
}

// n = side of the pattern, e.g. 3.
PatternPrevalence extract_patterns(const PalettedImage& sample, Int n, bool periodic_in, size_t symmetry)
{
	CHECK_LE_F(n, sample.width);
	CHECK_LE_F(n, sample.height);

	const auto pattern_from_sample = [&](size_t x, size_t y) {
		return make_pattern(n, [&](size_t dx, size_t dy){ return sample.at_wrapped(x + dx, y + dy); });
	};
	const auto rotate  = [&](const Pattern& p){ return make_pattern(n, [&](size_t x, size_t y){ return p[n - 1 - y + x * n]; }); };
	const auto reflect = [&](const Pattern& p){ return make_pattern(n, [&](size_t x, size_t y){ return p[n - 1 - x + y * n]; }); };

	PatternPrevalence patterns;

	auto add_pattern = [&](const Pattern& pattern)
	{
		patterns[hash_from_pattern(pattern, sample.palette.size())] += 1;
	};

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
				add_pattern(ps[k]);
			}
		}
	}

	return patterns;
}

Result observe(const Model& model, Output* output, RandomDouble& random_double)
{
	const auto log_t = std::log(model._num_patterns);

	std::vector<double> log_prob;
	for (const auto s : model._stationary) {
		log_prob.push_back(std::log(s));
	}

	double min = 1E+3;
	int argminx = -1, argminy = -1;

	for (int x = 0; x < model._width; ++x) {
		for (int y = 0; y < model._height; ++y) {
			if (model.on_boundary(x, y)) { continue; }

			size_t amount = 0;
			double sum = 0;

			for (int t = 0; t < model._num_patterns; ++t) {
				if (output->_wave(x, y, t)) {
					amount += 1;
					sum += model._stationary[t];
				}
			}

			if (sum == 0 || amount == 0) {
				return Result::kFail;
			}

			double entropy;

			if (amount == 1) {
				entropy = 0;
			} else if (amount == model._num_patterns) {
				entropy = log_t;
			} else {
				double main_sum = 0;
				double log_sum = std::log(sum);
				for (int t = 0; t < model._num_patterns; ++t) {
					if (output->_wave(x, y, t)) {
						main_sum += model._stationary[t] * log_prob[t];
					}
				}
				entropy = log_sum - main_sum / sum;
			}

			const double noise = 1E-6 * random_double();
			if (entropy > 0 && entropy + noise < min)
			{
				min = entropy + noise;
				argminx = x;
				argminy = y;
			}
		}
	}

	if (argminx == -1 && argminy == -1) {
		return Result::kSuccess;
	}

	std::vector<double> distribution(model._num_patterns);
	for (int t = 0; t < model._num_patterns; ++t) {
		distribution[t] = output->_wave(argminx, argminy, t) ? model._stationary[t] : 0;
	}
	size_t r = spin_the_bottle(std::move(distribution), random_double());
	for (int t = 0; t < model._num_patterns; ++t) {
		output->_wave(argminx, argminy, t) = (t == r);
	}
	output->_changes(argminx, argminy) = true;

	return Result::kUnfinished;
}


Result run(Output *output, const Model& model, size_t seed, size_t limit, const Palette& palette)
{
	output->_wave = Array3D<Bool>(model._width, model._height, model._num_patterns, true);
	output->_changes = Array2D<Bool>(model._width, model._height, false);

	std::mt19937 gen(seed);
	std::uniform_real_distribution<double> dis(0.0, 1.0);
	RandomDouble random_double = [&]() { return dis(gen); };

	for (size_t l = 0; l < limit || limit == 0; ++l) {
		Result result = observe(model, output, random_double);
		if (result != Result::kUnfinished) {
			LOG_F(INFO, "%s after %lu iterations", result2str(result), l);
			return result;
		}
		while (model.propagate(output));

		// const auto out_path = emilib::strprintf("output/simple_knot_%d.png", l);
		// const auto graphics = model.graphics(*output);
		// const auto rgba = rgba_from_graphics(graphics, palette);
		// stbi_write_png(out_path.c_str(), model._width, model._height, 4, rgba.data(), 0);
	}

	LOG_F(INFO, "Unfinished after %lu iterations", limit);
	return Result::kUnfinished;
}

void run_overlapping(const configuru::Config& config)
{
	const auto name = config["name"].as_string();
	const auto in_path = emilib::strprintf("samples/%s.bmp", name.c_str());

	const Int    n            = config.get_or("N",             3);
	const size_t out_width    = config.get_or("width",        48);
	const size_t out_height   = config.get_or("height",       48);
	const size_t symmetry     = config.get_or("symmetry",      8);
	const bool   periodic_out = config.get_or("periodic_out", true);
	const bool   periodic_in  = config.get_or("periodic_in",  true);
	const size_t foundation   = config.get_or("foundation",    8);
	const size_t limit        = config.get_or("limit",         0);

	const auto sample_image = load_paletted_image(in_path.c_str());
	LOG_F(INFO, "palette size: %lu", sample_image.palette.size());
	const auto hashed_patterns = extract_patterns(sample_image, n, periodic_in, symmetry);
	LOG_F(INFO, "Found %lu unique patterns in sample image", hashed_patterns.size());

	std::unique_ptr<const Model> model(new OverlappingModel(hashed_patterns, n, sample_image.palette.size(), periodic_out, out_width, out_height, foundation));

	for (const auto i : irange(config.get_or("screenshots", 2))) {
		for (const auto attempt : irange(10)) {
			(void)attempt;
			int seed = rand();

			Output output;
			const auto result = run(&output, *model, seed, limit, sample_image.palette);

			if (result == Result::kSuccess) {
				const auto graphics = model->graphics(output);
				const auto rgba = rgba_from_graphics(graphics, sample_image.palette);
				const auto out_path = emilib::strprintf("output/%s_%d.png", name.c_str(), i);
				CHECK_F(stbi_write_png(out_path.c_str(), out_width, out_height, 4, rgba.data(), 0) != 0,
				        "Failed to write image to %s", out_path.c_str());
				break;
			}
		}
	}
}

int main(int argc, char* argv[])
{
	loguru::init(argc, argv);

	const auto samples = configuru::parse_file("samples.cfg", configuru::CFG);
	for (const auto& p : samples["overlapping"].as_array()) {
		LOG_SCOPE_F(INFO, "%s", p["name"].c_str());
		run_overlapping(p);
	}
}

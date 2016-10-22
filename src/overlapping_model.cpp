#include "overlapping_model.hpp"

Pattern pattern_from_hash(const PatternHash hash, int n, size_t palette_size)
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

OverlappingModel::OverlappingModel(
	const PatternPrevalence& hashed_patterns,
	const Palette&           palette,
	int                      n,
	bool                     periodic_out,
	size_t                   width,
	size_t                   height,
	PatternHash              foundation_pattern)
{
	_width        = width;
	_height       = height;
	_num_patterns = hashed_patterns.size();
	_periodic_out = periodic_out;
	_n            = n;
	_palette      = palette;

	for (const auto& it : hashed_patterns) {
		if (it.first == foundation_pattern) {
			_foundation = _patterns.size();
		}

		_patterns.push_back(pattern_from_hash(it.first, n, _palette.size()));
		_pattern_weight.push_back(it.second);
	}

	const auto agrees = [&](const Pattern& p1, const Pattern& p2, int dx, int dy) {
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

	_propagator = Array3D<std::vector<PatternIndex>>(_num_patterns, 2 * n - 1, 2 * n - 1, {});

	size_t longest_propagator = 0;
	size_t sum_propagator = 0;

	for (auto t : irange(_num_patterns)) {
		for (auto x : irange<int>(2 * n - 1)) {
			for (auto y : irange<int>(2 * n - 1)) {
				auto& list = _propagator.mut_ref(t, x, y);
				for (auto t2 : irange(_num_patterns)) {
					if (agrees(_patterns[t], _patterns[t2], x - n + 1, y - n + 1)) {
						list.push_back(t2);
					}
				}
				list.shrink_to_fit();
				longest_propagator = std::max(longest_propagator, list.size());
				sum_propagator += list.size();
			}
		}
	}

	LOG_F(INFO, "propagator length: mean/max/sum: %.1f, %lu, %lu",
	    (double)sum_propagator / _propagator.size(), longest_propagator, sum_propagator);
}

bool OverlappingModel::propagate(Output* output) const
{
	bool did_change = false;

	for (int x1 = 0; x1 < _width; ++x1) {
		for (int y1 = 0; y1 < _height; ++y1) {
			if (!output->_changes.get(x1, y1)) { continue; }
			output->_changes.set(x1, y1, false);

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
						if (!output->_wave.get(sx, sy, t2)) { continue; }

						bool can_pattern_fit = false;

						const auto& prop = _propagator.ref(t2, _n - 1 - dx, _n - 1 - dy);
						for (const auto& t3 : prop) {
							if (output->_wave.get(x1, y1, t3)) {
								can_pattern_fit = true;
								break;
							}
						}

						if (!can_pattern_fit) {
							output->_changes.set(sx, sy, true);
							output->_wave.set(sx, sy, t2, false);
							did_change = true;
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
			auto& tile_constributors = result.mut_ref(x, y);

			for (int dy = 0; dy < _n; ++dy) {
				for (int dx = 0; dx < _n; ++dx) {
					int sx = x - dx;
					if (sx < 0) sx += _width;

					int sy = y - dy;
					if (sy < 0) sy += _height;

					if (on_boundary(sx, sy)) { continue; }

					for (int t = 0; t < _num_patterns; ++t) {
						if (output._wave.get(sx, sy, t)) {
							tile_constributors.push_back(_patterns[t][dx + dy * _n]);
						}
					}
				}
			}
		}
	}
	return result;
}

Image OverlappingModel::image(const Output& output) const
{
	return upsample(image_from_graphics(graphics(output), _palette));
}


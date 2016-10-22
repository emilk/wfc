#include <unordered_set>
#include <emilib/strprintf.hpp>
#include "tile_model.hpp"
#include "tile.hpp"

TileModel::TileModel(
    const configuru::Config& config,
    std::string subset_name,
    int width,
    int height,
    bool periodic_out,
    const TileLoader& tile_loader
) {
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

bool TileModel::propagate(Output* output) const
{
	bool did_change = false;

	for (int x2 = 0; x2 < _width; ++x2) {
		for (int y2 = 0; y2 < _height; ++y2) {
			for (int d = 0; d < 4; ++d) {
				int x1 = x2, y1 = y2;
				if (d == 0) {
					if (x2 == 0) {
						if (!_periodic_out) { continue; }
						x1 = _width - 1;
					} else {
						x1 = x2 - 1;
					}
				} else if (d == 1) {
					if (y2 == _height - 1) {
						if (!_periodic_out) { continue; }
						y1 = 0;
					} else {
						y1 = y2 + 1;
					}
				} else if (d == 2) {
					if (x2 == _width - 1) {
						if (!_periodic_out) { continue; }
						x1 = 0;
					} else {
						x1 = x2 + 1;
					}
				} else {
					if (y2 == 0) {
						if (!_periodic_out) { continue; }
						y1 = _height - 1;
					} else {
						y1 = y2 - 1;
					}
				}

				if (!output->_changes.get(x1, y1)) { continue; }

				for (int t2 = 0; t2 < _num_patterns; ++t2) {
					if (output->_wave.get(x2, y2, t2)) {
						bool b = false;
						for (int t1 = 0; t1 < _num_patterns && !b; ++t1) {
							if (output->_wave.get(x1, y1, t1)) {
								b = _propagator.get(d, t1, t2);
							}
						}
						if (!b) {
							output->_wave.set(x2, y2, t2, false);
							output->_changes.set(x2, y2, true);
							did_change = true;
						}
					}
				}
			}
		}
	}

	return did_change;
}

Image TileModel::image(const Output& output) const
{
	Image result(_width * _tile_size, _height * _tile_size, {});

	for (int x = 0; x < _width; ++x) {
		for (int y = 0; y < _height; ++y) {
			double sum = 0;
			for (const auto t : irange(_num_patterns)) {
				if (output._wave.get(x, y, t)) {
					sum += _pattern_weight[t];
				}
			}

			for (int yt = 0; yt < _tile_size; ++yt) {
				for (int xt = 0; xt < _tile_size; ++xt) {
					if (sum == 0) {
						result.set(x * _tile_size + xt, y * _tile_size + yt, RGBA{0, 0, 0, 255});
					} else {
						double r = 0, g = 0, b = 0, a = 0;
						for (int t = 0; t < _num_patterns; ++t) {
							if (output._wave.get(x, y, t)) {
								RGBA c = _tiles[t][xt + yt * _tile_size];
								r += (double)c.r * _pattern_weight[t] / sum;
								g += (double)c.g * _pattern_weight[t] / sum;
								b += (double)c.b * _pattern_weight[t] / sum;
								a += (double)c.a * _pattern_weight[t] / sum;
							}
						}

						result.set(x * _tile_size + xt, y * _tile_size + yt,
						           RGBA{(uint8_t)r, (uint8_t)g, (uint8_t)b, (uint8_t)a});
					}
				}
			}
		}
	}

	return result;
}


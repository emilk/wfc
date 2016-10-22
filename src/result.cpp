#include <limits>
#include <random>
#include "result.hpp"
#include "model.hpp"
#include "output.hpp"
#include "image.hpp"

const char* to_string(const Result result)
{
	return result == Result::kSuccess ? "success"
	     : result == Result::kFail    ? "fail"
	     : "unfinished";
}

Result find_lowest_entropy(
    const Model& model,
    const Output& output,
    RandomDouble& random_double,
    int* argminx,
    int* argminy
) {
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

			LOG_F(INFO, "%s after %lu iterations", to_string(result), l);
			return result;
		}
		while (model.propagate(output));
	}

	LOG_F(INFO, "Unfinished after %lu iterations", limit);
	return Result::kUnfinished;
}

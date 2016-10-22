#ifndef WFC_RESULT_HPP
#define WFC_RESULT_HPP

#include "model.hpp"
#include "output.hpp"

#define JO_GIF_HEADER_FILE_ONLY
#include <jo_gif.cpp>

enum class Result
{
	kSuccess,
	kFail,
	kUnfinished,
};

Result find_lowest_entropy(
    const Model& model,
    const Output& output,
    RandomDouble& random_double,
    int* argminx,
    int* argminy
);

Result observe(const Model& model, Output* output, RandomDouble& random_double);
Result run(Output* output, const Model& model, size_t seed, size_t limit, jo_gif_t* gif_out);

#endif


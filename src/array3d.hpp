#ifndef WFC_ARRAY3D_HPP
#define WFC_ARRAY3D_HPP

#include <vector>
#include <loguru.hpp>

template<typename T>
struct Array3D
{
public:
	Array3D() : _width(0), _height(0), _depth(0) {}
	Array3D(size_t w, size_t h, size_t d, T value = {})
		: _width(w), _height(h), _depth(d), _data(w * h * d, value) {}

	const size_t index(size_t x, size_t y, size_t z) const
	{
		DCHECK_LT_F(x, _width);
		DCHECK_LT_F(y, _height);
		DCHECK_LT_F(z, _depth);
		// return z * _width * _height + y * _width + x;
		return x * _height * _depth + y * _depth + z; // better cache hit ratio in our use case
	}

	inline       T& mut_ref(size_t x, size_t y, size_t z)       { return _data[index(x, y, z)]; }
	inline const T&     ref(size_t x, size_t y, size_t z) const { return _data[index(x, y, z)]; }
	inline       T      get(size_t x, size_t y, size_t z) const { return _data[index(x, y, z)]; }
	inline void set(size_t x, size_t y, size_t z, const T& value) { _data[index(x, y, z)] = value; }

	inline size_t size() const { return _data.size(); }

private:
	size_t _width, _height, _depth;
	std::vector<T> _data;
};

#endif


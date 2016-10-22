#ifndef WFC_ARRAY2D_HPP
#define WFC_ARRAY2D_HPP

#include <vector>
#include <loguru.hpp>

template<typename T>
struct Array2D
{
public:
	Array2D() : _width(0), _height(0) {}
	Array2D(size_t w, size_t h, T value = {})
		: _width(w), _height(h), _data(w * h, value) {}

	const size_t index(size_t x, size_t y) const
	{
		DCHECK_LT_F(x, _width);
		DCHECK_LT_F(y, _height);
		return y * _width + x;
	}

	inline       T& mut_ref(size_t x, size_t y)       { return _data[index(x, y)]; }
	inline const T&     ref(size_t x, size_t y) const { return _data[index(x, y)]; }
	inline       T      get(size_t x, size_t y) const { return _data[index(x, y)]; }
	inline void set(size_t x, size_t y, const T& value) { _data[index(x, y)] = value; }

	size_t   width()  const { return _width;       }
	size_t   height() const { return _height;      }
	const T* data()   const { return _data.data(); }

private:
	size_t _width, _height;
	std::vector<T> _data;
};

#endif


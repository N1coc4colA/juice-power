#ifndef JP_ALGORITHMS_H
#define JP_ALGORITHMS_H

#include <vector>
#include <array>
#include <ranges>

#include <glm/fwd.hpp>


namespace algorithms
{

template<typename T>
class MatrixView
{
public:
	constexpr
	MatrixView(T *data, size_t width, size_t height)
		: _data(data)
		, _width(width)
		, _height(height)
	{
	}

	constexpr
	inline size_t width() const
	{
		return _width;
	}

	constexpr
	inline size_t height() const
	{
		return _height;
	}

	constexpr
	inline auto get(const size_t h, const size_t w)
	{
		return _data[h*_width + w];
	}

	constexpr
	inline auto operator[](const size_t h)
	{
		return std::span(_data[h*_width], _width);
	}

	constexpr
	inline auto operator[](const size_t h) const
	{
		return std::span(_data[h*_width], _width);
	}

	constexpr
	inline const T *data() const
	{
		return _data;
	}

	constexpr
	inline T *data()
	{
		return _data;
	}

private:
	T *_data;
	size_t _width;
	size_t _height;
};


std::vector<glm::vec2> determineImageBorders(const MatrixView<unsigned char> &image);


}


#endif // JP_ALGORITHMS_H

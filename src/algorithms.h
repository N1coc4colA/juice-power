#ifndef JP_ALGORITHMS_H
#define JP_ALGORITHMS_H

#include <vector>
#include <span>

#include <glm/vec2.hpp>


typedef struct potrace_param_s potrace_param_t;


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
	inline std::span<T> operator[](const size_t h)
	{
		return std::span<T>(&_data[h*_width], _width);
	}

	constexpr
	inline std::span<const T> operator[](const size_t h) const
	{
		return std::span<const T>(&_data[h*_width], _width);
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

	constexpr
	inline std::span<T> flattened()
	{
		return std::span<T>(_data, _width*_height);
	}

	constexpr
	inline std::span<const T> flattened() const
	{
		return std::span<const T>(_data, _width*_height);
	}

private:
	T *_data = nullptr;
	size_t _width = 0;
	size_t _height = 0;
};


class ImageVectorizer
{
public:
	ImageVectorizer();
	~ImageVectorizer();

	ImageVectorizer(const ImageVectorizer &) = delete;
	ImageVectorizer(ImageVectorizer &&) = delete;

	ImageVectorizer &operator =(const ImageVectorizer &) const = delete;
	ImageVectorizer &operator =(ImageVectorizer &&) const = delete;

	/**
	 * @brief determineImageBorders
	 * @param image
	 * Data layout is RBGA, each channel with 8 bits, in pixel order, row-major order.
	 * This means that if your image is WxH, the input image must be (W*4)xH
	 */
	void determineImageBorders(const MatrixView<unsigned char> &image, int channelsCount);

	/**
	 *  @brief Resulting delimitation of the previous @fn determineImageBorders call.
	 *  Vector for the points delimiting the object.
	 *  Every point's location is normalized in the image's range.
	 *  As every point is just following in order, but we may have 2 groups,
	 *  when points switch from one group to another, the normal is null.
	 */
	std::vector<glm::vec2> points {};
	/// @brief Resulting normals of the previous @fn determineImageBorders call.
	std::vector<glm::vec2> normals {};

	glm::vec2 min {};
	glm::vec2 max {};

private:
    potrace_param_t *m_params = nullptr;

    /**
	 * @brief Continous data storage.
	 * This is used to store data for Potrace, and is re-used through the different
	 * calls to @fn determineImageBorders.
	 */
    std::vector<unsigned char> m_memory{};
};


}


#endif // JP_ALGORITHMS_H

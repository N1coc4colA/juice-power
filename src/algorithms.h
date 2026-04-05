#ifndef JP_ALGORITHMS_H
#define JP_ALGORITHMS_H

#include "keywords.h"

#include <vector>
#include <span>

#include <glm/vec2.hpp>

#include <cstddef>

#include "aligned_vector.h"

using potrace_param_t = struct potrace_param_s;
using potrace_word = unsigned long;

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

    constexpr inline auto get(const size_t h, const size_t w)
    {
        assert(h <= _height);
        assert(w < _width);

        return _data[h * _width + w];
    }

    constexpr inline auto operator[](const size_t h)
    {
        assert(h <= _height);
        return std::span<T>(&_data[h * _width], _width);
    }

    constexpr inline auto operator[](const size_t h) const
    {
        assert(h <= _height);
        return std::span<const T>(&_data[h * _width], _width);
    }

    _nodiscard constexpr inline auto data() { return _data; }
    _nodiscard constexpr inline auto data() const { return _data; }

    _nodiscard constexpr inline auto flattened() { return std::span<T>(_data, _width * _height); }
    _nodiscard constexpr inline auto flattened() const { return std::span<const T>(_data, _width * _height); }

    _nodiscard constexpr inline auto width() const -> size_t { return _width; }
    _nodiscard constexpr inline auto height() const -> size_t { return _height; }

private:
	T *_data = nullptr;
	size_t _width = 0;
	size_t _height = 0;
};


class ImageVectorizer
{
public:
    static constexpr int turdSize = 10;    // ignore small regions
    static constexpr float alphaMax = 1.0; // corner threshold
    static constexpr int transparencyLimit = 100;

    ImageVectorizer();
    ImageVectorizer(const ImageVectorizer &) = delete;
    ImageVectorizer(ImageVectorizer &&) = delete;
    ~ImageVectorizer();

    auto operator=(const ImageVectorizer &) const -> ImageVectorizer & = delete;
    auto operator=(ImageVectorizer &&) const -> ImageVectorizer & = delete;

    /**
	 * @brief determineImageBorders
	 * @param image
	 * Data layout is RBGA, each channel with 8 bits, in pixel order, row-major order.
	 * This means that if your image is WxH, the input image must be (W*4)xH
	 */
    void determineImageBorders(const MatrixView<unsigned char> &image, const int channelsCount);

    /**
	 *  @brief Resulting delimitation of the previous @fn determineImageBorders call.
	 *  Vector for the points delimiting the object.
	 *  Every point's location is normalized in the image's range.
	 *  As every point is just following in order, but we may have 2 groups,
	 *  when points switch from one group to another, the normal is null.
	 */
    _nodiscard inline auto getPoints() const -> const std::vector<glm::vec2> & { return points; }
    /// @brief Resulting normals of the previous @fn determineImageBorders call.
    _nodiscard inline auto getNormals() const -> const std::vector<glm::vec2> & { return normals; }

    _nodiscard inline auto getMin() const { return min; }
    _nodiscard inline auto getMax() const { return max; }

private:
    potrace_param_t *m_params = nullptr;

    /**
	 * @brief Continous data storage.
	 * This is used to store data for Potrace, and is re-used through the different
	 * calls to @fn determineImageBorders.
	 */
    TypeAlignedVector<unsigned char, potrace_word> m_memory{};

    std::vector<glm::vec2> points{};
    std::vector<glm::vec2> normals{};

    glm::vec2 min{};
    glm::vec2 max{};
};


}


#endif // JP_ALGORITHMS_H

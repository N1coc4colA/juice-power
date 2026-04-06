#ifndef JP_ALGORITHMS_H
#define JP_ALGORITHMS_H

#include <glm/vec2.hpp>

#include <vector>
#include <span>

#include "src/aligned_vector.h"
#include "src/keywords.h"

/// @brief Opaque Potrace parameter type forward declaration alias.
using potrace_param_t = struct potrace_param_s;
/// @brief Potrace bitmap word storage type.
using potrace_word = unsigned long;

namespace algorithms
{

/**
 * @brief Lightweight 2D view over contiguous row-major storage.
 * @tparam T Element type.
 */
template<typename T>
class MatrixView
{
public:
	/// @brief Creates a matrix view over a contiguous buffer.
    constexpr MatrixView(T *data, const size_t width, const size_t height)
        : m_data(data)
        , m_width(width)
        , m_height(height)
    {}

    /// @brief Returns element value at row h and column w.
    constexpr auto get(const size_t h, const size_t w)
    {
        assert(h <= m_height);
        assert(w < m_width);

        return m_data[h * m_width + w];
    }

    /// @brief Returns mutable span for one row.
    constexpr auto operator[](const size_t h)
    {
        assert(h <= m_height);
        return std::span<T>(&m_data[h * m_width], m_width);
    }

    /// @brief Returns const span for one row.
    constexpr auto operator[](const size_t h) const
    {
        assert(h <= m_height);
        return std::span<const T>(&m_data[h * m_width], m_width);
    }

    /// @brief Returns raw mutable data pointer.
    _nodiscard constexpr auto data() { return m_data; }
    /// @brief Returns raw const data pointer.
    _nodiscard constexpr auto data() const { return m_data; }

    /// @brief Returns mutable flattened view over all cells.
    _nodiscard constexpr auto flattened() { return std::span<T>(m_data, m_width * m_height); }
    /// @brief Returns const flattened view over all cells.
    _nodiscard constexpr auto flattened() const { return std::span<const T>(m_data, m_width * m_height); }

    /// @brief Matrix width in elements.
    _nodiscard constexpr auto width() const -> size_t { return m_width; }
    /// @brief Matrix height in elements.
    _nodiscard constexpr auto height() const -> size_t { return m_height; }

private:
	/// @brief Backing storage pointer.
    T *m_data = nullptr;
    /// @brief Number of columns.
    size_t m_width = 0;
    /// @brief Number of rows.
    size_t m_height = 0;
};


/**
 * @brief Converts raster images into border and normal vectors.
 */
class ImageVectorizer
{
public:
    /// @brief Minimum connected component size kept during vectorization.
    static constexpr int turdSize = 10;
    /// @brief Corner detection threshold used by Potrace.
    static constexpr float alphaMax = 1.0;
    /// @brief Alpha threshold under which pixels are considered transparent.
    static constexpr int transparencyLimit = 100;

    /// @brief Initializes Potrace parameters.
    ImageVectorizer();
    /// @brief Non-copyable because Potrace params are owned resources.
    ImageVectorizer(const ImageVectorizer &) = delete;
    /// @brief Non-movable because internal views point to owned data.
    ImageVectorizer(ImageVectorizer &&) = delete;
    /// @brief Releases Potrace resources.
    ~ImageVectorizer();

    /// @brief Non-assignable.
    auto operator=(const ImageVectorizer &) const -> ImageVectorizer & = delete;
    /// @brief Non-assignable.
    auto operator=(ImageVectorizer &&) const -> ImageVectorizer & = delete;

    /**
	 * @brief determineImageBorders
	 * @param image The image's data buffer, stored a W*H*(R,B,B[,A])
	 * @param channelsCount Number of channels as in R,G,B,A, most likely 3 or 4.
	 * Data layout is RBGA, each channel with 8 bits, in pixel order, row-major order.
	 * This means that if your image is WxH, the input image must be (W*4)xH
	 */
    void determineImageBorders(const MatrixView<unsigned char> &image, int channelsCount);

    /**
	 *  @brief Resulting delimitation of the previous @fn determineImageBorders call.
	 *  Vector for the m_points delimiting the object.
	 *  Every point's location is normalized in the image's range.
	 *  As every point is just following in order, but we may have 2 groups,
	 *  when m_points switch from one group to another, the normal is null.
	 */
    _nodiscard auto getPoints() const -> const std::vector<glm::vec2> & { return m_points; }
    /// @brief Resulting normals of the previous @fn determineImageBorders call.
    _nodiscard auto getNormals() const -> const std::vector<glm::vec2> & { return normals; }

    /// @brief Returns minimum border coordinate.
    _nodiscard auto getMin() const { return m_min; }
    /// @brief Returns maximum border coordinate.
    _nodiscard auto getMax() const { return m_max; }

private:
    /// @brief Potrace parameter block.
    potrace_param_t *m_params = nullptr;

    /**
	 * @brief Reusable contiguous bitmap storage passed to Potrace.
	 * This is used to store data for Potrace, and is re-used through the different
	 * calls to @fn determineImageBorders.
	 */
    TypeAlignedVector<unsigned char, potrace_word> m_memory{};

    /// @brief Extracted border m_points.
    std::vector<glm::vec2> m_points{};
    /// @brief Extracted normals associated to points.
    std::vector<glm::vec2> normals{};

    /// @brief Minimum coordinate among extracted points.
    glm::vec2 m_min{};
    /// @brief Maximum coordinate among extracted points.
    glm::vec2 m_max{};
};


}


#endif // JP_ALGORITHMS_H

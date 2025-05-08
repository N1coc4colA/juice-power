#include "algorithms.h"

#include <potracelib.h>


/*
#define BM_USET(bm, x, y) (*bm_index(bm, x, y) |= bm_mask(x))
#define BM_UCLR(bm, x, y) (*bm_index(bm, x, y) &= ~bm_mask(x))
#define BM_UPUT(bm, x, y, b) ((b) ? BM_USET(bm, x, y) : BM_UCLR(bm, x, y))
#define BM_PUT(bm, x, y, b) (bm_safe(bm, x, y) ? BM_UPUT(bm, x, y, b) : 0)
*/


constexpr const auto po_ws = sizeof(potrace_word);
constexpr const auto po_wbs = sizeof(potrace_word)*8;
constexpr const auto po_wbs_sub = po_wbs -1;


namespace algorithms
{

ImageVectorizer::ImageVectorizer()
    : params(potrace_param_default())
{
	params->turdsize = 10; // ignore small regions
	params->alphamax = 1.0; // corner threshold
}

ImageVectorizer::~ImageVectorizer()
{
	potrace_param_free(params);
}

void ImageVectorizer::determineImageBorders(const MatrixView<unsigned char> &image, int channelsCount)
{
	points.clear();
	normals.clear();

	const auto &imageWidth = image.width();

	// If we have only 3 channels, this means that there is no alpha channel, so the border's just the image.
	if (channelsCount == 3) {
		normals.resize(4);
		points.resize(4);

		// Up, right, down, left.
		normals[0] = glm::vec2{0.f, 1.f};
		normals[1] = glm::vec2{1.f, 0.f};
		normals[2] = glm::vec2{0.f, -1.f};
		normals[3] = glm::vec2{-1.f, 0.f};

		points[0] = glm::vec2{0.f, 0.f};
		points[1] = glm::vec2{0.f, 1.f};
		points[2] = glm::vec2{1.f, 1.f};
		points[3] = glm::vec2{1.f, 0.f};

		return;
	}

	const size_t imgRealWidth = imageWidth/4;

	// Words per line.
	const auto dy = (imgRealWidth + po_wbs_sub) / po_wbs;
	// Perform resize. Our storage is per-byte.
	memory.resize(dy * image.height() * po_ws);

	const potrace_bitmap_t bm {
	    .w = static_cast<int>(image.width()),
	    .h = static_cast<int>(image.height()),
	    .dy = static_cast<int>(dy),
	    .map = reinterpret_cast<potrace_word *>(memory.data()),
	};

	const auto img = image.flattened();

	for (size_t y = 0; y < image.height(); y++) {
		for (size_t x = 0; x < imgRealWidth; x++) {
			const auto wi = y*dy + x / po_wbs;
			const auto bi = x % po_wbs;
			const auto val = img[y*size_t(image.height()) + x*4 +3];

			// Set to 1 or 0 depending on the transparency.
			if (val > 0x80) {
				memory[wi] |= 1 << bi;
			} else {
				memory[wi] &= ~(1 << bi);
			}
		}
	}

	// Perform trace
	potrace_state_t *st = potrace_trace(params, &bm);

	// If we have no curve, it means that all the image is non-transparent.
	if (!st->plist) {
		normals.resize(4);
		points.resize(4);

		// Up, right, down, left.
		normals[0] = glm::vec2{0.f, 1.f};
		normals[1] = glm::vec2{1.f, 0.f};
		normals[2] = glm::vec2{0.f, -1.f};
		normals[3] = glm::vec2{-1.f, 0.f};

		points[0] = glm::vec2{0.f, 0.f};
		points[1] = glm::vec2{0.f, 1.f};
		points[2] = glm::vec2{1.f, 1.f};
		points[3] = glm::vec2{1.f, 0.f};

		return;
	}

	// Convert to vector paths

	/*
	 * We use simple physics. We just need points to delimit the area of the image.
	 * For this purpose, we count any corner. However, for each bezier curve,
	 * the reality is that we just use the ctl points.
	 *
	 */

	// We just have to walk through the top level paths, which are always outter-directed.
	{
		size_t pointsCount = 0;
		const potrace_path_t *path = st->plist;

		while (path) {
			const potrace_curve_t &curve = path->curve;

			for (auto i = 0; i < curve.n; i++) {
				if (curve.tag[i] == POTRACE_CORNER) {
					pointsCount++;
				} else if (curve.tag[i] == POTRACE_CURVETO) {
					pointsCount += 2;
				}
			}

			path = path->next;
		}

		points.resize(pointsCount);
		normals.resize(pointsCount -1);
	}

	{
		const potrace_path_t *path = st->plist;
		while (path) {
			const potrace_curve_t &curve = path->curve;

			for (auto i = 0; i < curve.n; i++) {
				if (curve.tag[i] == POTRACE_CORNER) {
					points.push_back(glm::vec2{curve.c[i][1].x, curve.c[i][1].y});
				} else if (curve.tag[i] == POTRACE_CURVETO) {
					points.push_back(glm::vec2{curve.c[i][0].x, curve.c[i][0].y});
					points.push_back(glm::vec2{curve.c[i][1].x, curve.c[i][1].y});
				}
			}

			path = path->next;
		}
	}

	// Determine the normals.
	{
		auto p = 0;
		const potrace_path_t *path = st->plist;
		while (path->next) {
			const potrace_curve_t &curve = path->curve;

			for (auto i = 0; i < curve.n -1; i++) {
				auto vec = points[p+1] - points[p];
				const auto len = vec.length();
				// Normalize
				vec /= len;

				// Add perpendicular vector.
				normals.push_back(glm::vec2{-vec.y, vec.x});

				p++;
			}

			normals.push_back(glm::vec2{0.f, 0.f});
			p++;

			path = path->next;
		}

		// Process the last curve.
		const potrace_curve_t &curve = path->curve;

		for (auto i = 0; i < curve.n -1; i++) {
			auto vec = points[p+1] - points[p];
			const auto len = vec.length();
			// Normalize
			vec /= len;

			// Add perpendicular vector.
			normals.push_back(glm::vec2{-vec.y, vec.x});

			p++;
		}
	}

	// Normalize the points within the image.
	for (auto &p : points) {
		p.x /= static_cast<float>(imgRealWidth);
	}

	const float height = static_cast<float>(image.height());
	for (auto &p : points) {
		p.y /= height;
	}

	potrace_state_free(st);
}


}

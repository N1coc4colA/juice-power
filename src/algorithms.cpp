#include "algorithms.h"

#include <glm/geometric.hpp>
#include <potracelib.h>

#include <iostream>

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
    : m_params(potrace_param_default())
{
	m_params->turdsize = 10; // ignore small regions
	m_params->alphamax = 1.0; // corner threshold
}

ImageVectorizer::~ImageVectorizer()
{
	potrace_param_free(m_params);
}

void ImageVectorizer::determineImageBorders(const MatrixView<unsigned char> &image, const int channelsCount)
{
	points.clear();
	normals.clear();

    const auto &imageWidth = image.width() / channelsCount;
    std::cout << "Image width: " << imageWidth << '\n';

    // If we have only 3 channels, this means that there is no alpha channel, so the border's just the image.
    if (channelsCount == 3) {
        normals.resize(4);
        points.resize(5);

        // Up, right, down, left.
        normals[0] = glm::vec2{-1.f, 0.f};
        normals[1] = glm::vec2{0.f, 1.f};
        normals[2] = glm::vec2{1.f, 0.f};
        normals[3] = glm::vec2{0.f, -1.f};

        points[0] = glm::vec2{0.f, 0.f};
        points[1] = glm::vec2{0.f, 1.f};
        points[2] = glm::vec2{1.f, 1.f};
        points[3] = glm::vec2{1.f, 0.f};
        points[4] = glm::vec2{0.f, 0.f};

        min = glm::vec2{0.f, 0.f};
        max = glm::vec2{1.f, 1.f};

        return;
    }

    const size_t imgRealWidth = imageWidth;

    // Words per line.
    const auto dy = (imgRealWidth + po_wbs_sub) / po_wbs;
    // Perform resize. Our storage is per-byte.
    m_memory.resize(dy * image.height() * po_ws);

    const potrace_bitmap_t bm{
        .w = static_cast<int>(imgRealWidth),
        .h = static_cast<int>(image.height()),
        .dy = static_cast<int>(dy),
        .map = reinterpret_cast<potrace_word *>(m_memory.data()),
    };

    const auto img = image.flattened();

    for (size_t y = 0; y < image.height(); y++) {
		for (size_t x = 0; x < imgRealWidth; x++) {
			const auto wi = y*dy + x / po_wbs;
			const auto bi = x % po_wbs;
			const auto val = img[y*size_t(image.height()) + x*4 +3];

			// Set to 1 or 0 depending on the transparency.
            if (val > 100) {
                m_memory[wi] |= 1 << bi;
            } else {
                m_memory[wi] &= ~(1 << bi);
            }
        }
    }

    // Perform trace
    potrace_state_t *st = potrace_trace(m_params, &bm);

    // If we have no curve, it means that all the image is non-transparent.
    if (!st->plist) {
        normals.resize(4);
        points.resize(5);

        // Up, right, down, left.
        normals[0] = glm::vec2{-1.f, 0.f};
        normals[1] = glm::vec2{0.f, 1.f};
        normals[2] = glm::vec2{1.f, 0.f};
        normals[3] = glm::vec2{0.f, -1.f};

        points[0] = glm::vec2{0.f, 0.f};
        points[1] = glm::vec2{0.f, 1.f};
        points[2] = glm::vec2{1.f, 1.f};
        points[3] = glm::vec2{1.f, 0.f};
        points[4] = glm::vec2{0.f, 0.f};

        min = glm::vec2{0.f, 0.f};
        max = glm::vec2{1.f, 1.f};

        assert(max.x <= 1.f && max.y <= 1.f);
        assert(0.f <= max.x && 0.f <= max.y);

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

        points.reserve(pointsCount + 1); // Due to enclosing point.
        normals.reserve(pointsCount);
    }

    /* Determine the points */ {
        const potrace_path_t *path = st->plist;
        // Track how many distinct points were added for each path so we can
        // correctly compute normals per-path (potrace curve.n does not match
        // the number of points we actually push when converting CURVETO->2 pts).
        // Move declaration to function scope: declare here but outside the inner
        // block so the normals block can access it. We'll reuse the same name.
    }

    // per-path distinct point counts and signs (visible to the normals computation below)
    std::vector<int> pathPointCounts;
    std::vector<int> pathSigns;

    /* Determine the points */ {
        const potrace_path_t *path = st->plist;
        // Track how many distinct points were added for each path so we can
        // correctly compute normals per-path (potrace curve.n does not match
        // the number of points we actually push when converting CURVETO->2 pts).
        int pathIndex = 0;
        while (path) {
            const potrace_curve_t &curve = path->curve;
            const int startIndex = static_cast<int>(points.size());

            int countThisPath = 0;
            for (auto i = 0; i < curve.n; i++) {
                if (curve.tag[i] == POTRACE_CORNER) {
                    points.push_back(glm::vec2{curve.c[i][1].x, curve.c[i][1].y});
                    countThisPath += 1;
                } else if (curve.tag[i] == POTRACE_CURVETO) {
                    points.push_back(glm::vec2{curve.c[i][0].x, curve.c[i][0].y});
                    points.push_back(glm::vec2{curve.c[i][1].x, curve.c[i][1].y});
                    countThisPath += 2;
                }
            }

            // If we added at least one point for this path, append a closing
            // point equal to the first point of the path so edge iteration
            // can use next = idx+1 without modulo complexity.
            if (countThisPath > 0) {
                const int firstIdx = startIndex;
                points.push_back(points[firstIdx]); // closing duplicate
            }

            pathPointCounts.push_back(countThisPath);
            // record the sign of this path: potrace uses sign>0 for CCW, <0 for CW
            pathSigns.push_back(path->sign);
             path = path->next;
             ++pathIndex;
         }
         // Save the per-path counts for the normals phase by attaching to a
         // temporary place accessible afterwards. We'll stash it in 'min' x as a
         // hack isn't needed because we're in the same scope below; instead,
         // we'll capture pathPointCounts by moving it out to a lambda scope.
         // To keep edits minimal, the vector 'pathPointCounts' will be used
         // directly in the normals-generation block below.
    }

    // Normals will be (re)computed after we normalize/scale points below so
    // they are in the same coordinate system used by the physics engine.
    // Keep pathPointCounts available for that phase.

    // Normalize the points within the image.
    const float width = static_cast<float>(imgRealWidth);
    for (auto &p : points) {
        p.x /= width;
    }

    const float height = static_cast<float>(image.height());
	for (auto &p : points) {
		p.y /= height;
	}

    // Recompute normals in final (normalized) coordinate system using our per-path counts.
    {
        normals.clear();
        const float eps2 = 1e-8f;
        size_t p = 0;

        for (size_t pathIndex = 0; pathIndex < pathPointCounts.size(); ++pathIndex) {
            const int npts = pathPointCounts[pathIndex];
            if (npts <= 0) {
                continue;
            }

            const size_t base = p;

            // centroid computed over the distinct points (exclude the closing dup)
            glm::vec2 centroid{0.f, 0.f};
            for (int i = 0; i < npts; ++i) {
                centroid += points[base + i];
            }
            centroid /= static_cast<float>(npts);

            glm::vec2 lastValidNormal{1.f, 0.f};

            for (int i = 0; i < npts; ++i) {
                const int idx = static_cast<int>(base) + i;
                const int next = static_cast<int>(base) + i + 1; // safe: we added closing point

                glm::vec2 edge = points[next] - points[idx];
                const float len2 = glm::dot(edge, edge);

                glm::vec2 n;
                if (len2 > eps2) {
                    n = glm::normalize(glm::vec2{-edge.y, edge.x});
                    lastValidNormal = n;
                } else {
                    n = lastValidNormal;
                }

                // Use Potrace path sign to ensure consistent outward normals.
                // Potrace: positive curves run CCW (interior to the left),
                // so left-perp is inward and must be flipped. For negative/CW
                // paths left-perp is outward.
                const int psign = (pathIndex < pathSigns.size()) ? pathSigns[pathIndex] : 0;
                if (psign > 0) {
                    n = -n;
                    lastValidNormal = n;
                }

                normals.push_back(n);
            }

            p += static_cast<size_t>(npts) + 1;
        }
    }

    min = glm::vec2{width, height};
    max = glm::vec2{0.f, 0.f};

    for (const auto &p : points) {
        if (p.x < min.x) {
            min.x = p.x;
        }
    }
    for (const auto &p : points) {
        if (p.y < min.y) {
			min.y = p.y;
		}
    }

    for (const auto &p : points) {
        if (p.x > max.x) {
			max.x = p.x;
		}
    }
    for (const auto &p : points) {
        if (p.y > max.y) {
			max.y = p.y;
		}
    }

    potrace_state_free(st);
}


}

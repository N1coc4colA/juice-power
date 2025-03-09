#include "loader.h"

#include <iostream>
#include <fmt/printf.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

#include <stb/stb_image.h>

#include <fastgltf/include/fastgltf/glm_element_traits.hpp>
#include <fastgltf/include/fastgltf/tools.hpp>
#include <fastgltf/include/fastgltf/core.hpp>

#include <magic_enum.hpp>

#include "engine.h"
#include "types.h"


std::optional<std::vector<std::shared_ptr<MeshAsset>>> loadGltfMeshes(Engine *engine, const std::filesystem::path &filePath)
{
	std::cout << "Loading GLTF: " << filePath << std::endl;

	auto result = fastgltf::GltfDataBuffer::FromPath(filePath);
	if (result.error() != fastgltf::Error::None) {
		std::cout << "ERROR: GLTF loading failed (" << magic_enum::enum_name(result.error()) << ") for: " << filePath << std::endl;
		return {};
	}

	fastgltf::GltfDataBuffer &data = result.get();

	constexpr auto gltfOptions = fastgltf::Options::None
		| fastgltf::Options::LoadGLBBuffers
		| fastgltf::Options::LoadExternalBuffers;

	fastgltf::Asset gltf;
	fastgltf::Parser parser {};

	auto load = parser.loadGltfBinary(data, filePath.parent_path(), gltfOptions);
	if (load) {
		gltf = std::move(load.get());
	} else {
		fmt::print("Failed to load glTF: {} \n", fastgltf::to_underlying(load.error()));
		return {};
	}

	std::vector<std::shared_ptr<MeshAsset>> meshes;

	// use the same vectors for all meshes so that the memory doesnt reallocate as
	// often
	std::vector<uint32_t> indices;
	std::vector<Vertex> vertices;
	for (fastgltf::Mesh &mesh : gltf.meshes) {
		MeshAsset newmesh;

		newmesh.name = mesh.name;

		// clear the mesh arrays each mesh, we dont want to merge them by error
		indices.clear();
		vertices.clear();

		for (auto &&p : mesh.primitives) {
			const GeoSurface newSurface {
				.startIndex = (uint32_t)indices.size(),
				.count = (uint32_t)gltf.accessors[p.indicesAccessor.value()].count,
			};

			const size_t initial_vtx = vertices.size();

			// load indexes
			{
				const fastgltf::Accessor &indexaccessor = gltf.accessors[p.indicesAccessor.value()];
				indices.reserve(indices.size() + indexaccessor.count);

				fastgltf::iterateAccessor<std::uint32_t>(
					gltf,
					indexaccessor,
					[&](std::uint32_t idx) {
						indices.push_back(idx + initial_vtx);
					}
				);
			}

			// load vertex positions
			{
				const fastgltf::Accessor &posAccessor = gltf.accessors[p.findAttribute("POSITION")->accessorIndex];
				vertices.resize(vertices.size() + posAccessor.count);

				fastgltf::iterateAccessorWithIndex<glm::vec3>(
					gltf,
					posAccessor,
					[&](glm::vec3 v, size_t index) {
						const Vertex newvtx {
							.position = v,
							.uv_x = 0,
							.normal = { 1, 0, 0 },
							.uv_y = 0,
							.color = glm::vec4 { 1.f },
						};
						vertices[initial_vtx + index] = newvtx;
					}
				);
			}

			// load vertex normals
			const auto normals = p.findAttribute("NORMAL");
			if (normals != p.attributes.end()) {
				fastgltf::iterateAccessorWithIndex<glm::vec3>(
					gltf,
					gltf.accessors[(*normals).accessorIndex],
					[&](glm::vec3 v, size_t index) {
						vertices[initial_vtx + index].normal = v;
					}
				);
			}

			// load UVs
			const auto uv = p.findAttribute("TEXCOORD_0");
			if (uv != p.attributes.end()) {
				fastgltf::iterateAccessorWithIndex<glm::vec2>(
					gltf,
					gltf.accessors[(*uv).accessorIndex],
					[&](glm::vec2 v, size_t index) {
						vertices[initial_vtx + index].uv_x = v.x;
						vertices[initial_vtx + index].uv_y = v.y;
					});
			}

			// load vertex colors
			const auto colors = p.findAttribute("COLOR_0");
			if (colors != p.attributes.end()) {
				fastgltf::iterateAccessorWithIndex<glm::vec4>(
					gltf, gltf.accessors[(*colors).accessorIndex],
					[&](glm::vec4 v, size_t index) {
						vertices[initial_vtx + index].color = v;
					}
				);
			}
			newmesh.surfaces.push_back(newSurface);
		}

		// display the vertex normals
		constexpr bool OverrideColors = true;
		if (OverrideColors) {
			for (Vertex &vtx : vertices) {
				vtx.color = glm::vec4(vtx.normal, 1.f);
			}
		}
		newmesh.meshBuffers = engine->uploadMesh(indices, vertices);

		meshes.emplace_back(std::make_shared<MeshAsset>(std::move(newmesh)));
	}

	return meshes;
}

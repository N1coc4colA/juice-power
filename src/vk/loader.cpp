#include "loader.h"
#include "src/vk/loadedgltf.h"

#include <iostream>
#include <fmt/printf.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

#include <stb/stb_image.h>

#include <fastgltf/include/fastgltf/glm_element_traits.hpp>
#include <fastgltf/include/fastgltf/tools.hpp>
#include <fastgltf/include/fastgltf/core.hpp>

#include <fmt/printf.h>

#include <magic_enum.hpp>

#include "engine.h"
#include "../graphics/engine.h"


template<typename Output, typename Source>
	requires (sizeof(Output) == sizeof(Source))
Output *sweep(Source *src)
{
	return reinterpret_cast<Output *>(src);
}

template<typename Output, typename Source>
	requires (sizeof(Output) == sizeof(Source))
const Output *sweep(const Source *src)
{
	return reinterpret_cast<const Output *>(src);
}


constexpr VkFilter extract_filter(const fastgltf::Filter filter)
{
	switch (filter) {
		// nearest samplers
		case fastgltf::Filter::Nearest:
		case fastgltf::Filter::NearestMipMapNearest:
		case fastgltf::Filter::NearestMipMapLinear: {
			return VK_FILTER_NEAREST;
		}

		// linear samplers
		case fastgltf::Filter::Linear:
		case fastgltf::Filter::LinearMipMapNearest:
		case fastgltf::Filter::LinearMipMapLinear:
		default: {
			return VK_FILTER_LINEAR;
		}
	}
}

constexpr VkSamplerMipmapMode extract_mipmap_mode(const fastgltf::Filter filter)
{
	switch (filter) {
		case fastgltf::Filter::NearestMipMapNearest:
		case fastgltf::Filter::LinearMipMapNearest: {
			return VK_SAMPLER_MIPMAP_MODE_NEAREST;
		}

		case fastgltf::Filter::NearestMipMapLinear:
		case fastgltf::Filter::LinearMipMapLinear:
		default: {
			return VK_SAMPLER_MIPMAP_MODE_LINEAR;
		}
	}
}

std::optional<std::vector<std::shared_ptr<MeshAsset>>> loadGltfMeshes(Graphics::Engine &engine, const std::filesystem::path &filePath)
{
	std::cout << "Loading GLTF: " << filePath << '\n';

	auto result = fastgltf::GltfDataBuffer::FromPath(filePath);
	if (result.error() != fastgltf::Error::None) {
		std::cout << "ERROR: GLTF loading failed (" << magic_enum::enum_name(result.error()) << ") for: " << filePath << '\n';
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

	std::vector<std::shared_ptr<MeshAsset>> meshes = {};

	// use the same vectors for all meshes so that the memory doesnt reallocate as
	// often
	std::vector<uint32_t> indices = {};
	std::vector<Vertex> vertices = {};
	for (const fastgltf::Mesh &mesh : gltf.meshes) {
		MeshAsset newmesh = {
			.name = std::string(mesh.name),
		};

		// clear the mesh arrays each mesh, we dont want to merge them by error
		indices.clear();
		vertices.clear();

		for (auto &&p : mesh.primitives) {
			const GeoSurface newSurface {
				.startIndex = static_cast<uint32_t>(indices.size()),
				.count = static_cast<uint32_t>(gltf.accessors[p.indicesAccessor.value()].count),
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
					[&](const glm::vec3 v, const size_t index) {
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
					[&](const glm::vec3 v, const size_t index) {
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
					[&](const glm::vec2 v, const size_t index) {
						vertices[initial_vtx + index].uv_x = v.x;
						vertices[initial_vtx + index].uv_y = v.y;
					}
				);
			}

			// load vertex colors
			const auto colors = p.findAttribute("COLOR_0");
			if (colors != p.attributes.end()) {
				fastgltf::iterateAccessorWithIndex<glm::vec4>(
					gltf, gltf.accessors[(*colors).accessorIndex],
					[&](const glm::vec4 v, const size_t index) {
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

		newmesh.meshBuffers = engine.uploadMesh(indices, vertices);

		meshes.emplace_back(std::make_shared<MeshAsset>(std::move(newmesh)));
	}

	return meshes;
}

std::optional<std::vector<std::shared_ptr<MeshAsset>>> loadGltfMeshes(Engine &engine, const std::filesystem::path &filePath)
{
	std::cout << "Loading GLTF: " << filePath << '\n';

	auto result = fastgltf::GltfDataBuffer::FromPath(filePath);
	if (result.error() != fastgltf::Error::None) {
		std::cout << "ERROR: GLTF loading failed (" << magic_enum::enum_name(result.error()) << ") for: " << filePath << '\n';
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

	std::vector<std::shared_ptr<MeshAsset>> meshes = {};

	// use the same vectors for all meshes so that the memory doesnt reallocate as
	// often
	std::vector<uint32_t> indices = {};
	std::vector<Vertex> vertices = {};
	for (const fastgltf::Mesh &mesh : gltf.meshes) {
		MeshAsset newmesh = {
		    .name = std::string(mesh.name),
		};

		// clear the mesh arrays each mesh, we dont want to merge them by error
		indices.clear();
		vertices.clear();

		for (auto &&p : mesh.primitives) {
			const GeoSurface newSurface {
			    .startIndex = static_cast<uint32_t>(indices.size()),
			    .count = static_cast<uint32_t>(gltf.accessors[p.indicesAccessor.value()].count),
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
				    [&](const glm::vec3 v, const size_t index) {
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
				    [&](const glm::vec3 v, const size_t index) {
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
				    [&](const glm::vec2 v, const size_t index) {
					    vertices[initial_vtx + index].uv_x = v.x;
					    vertices[initial_vtx + index].uv_y = v.y;
				    }
				    );
			}

			// load vertex colors
			const auto colors = p.findAttribute("COLOR_0");
			if (colors != p.attributes.end()) {
				fastgltf::iterateAccessorWithIndex<glm::vec4>(
				    gltf, gltf.accessors[(*colors).accessorIndex],
				    [&](const glm::vec4 v, const size_t index) {
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

		newmesh.meshBuffers = engine.uploadMesh(indices, vertices);

		meshes.emplace_back(std::make_shared<MeshAsset>(std::move(newmesh)));
	}

	return meshes;
}

std::optional<std::shared_ptr<LoadedGLTF>> loadGltf(Engine &engine, const std::filesystem::path &filePath)
{
	std::cout << "Loading GLTF: " << filePath << '\n';

	std::shared_ptr<LoadedGLTF> scene = std::make_shared<LoadedGLTF>();
	scene->creator = &engine;
	LoadedGLTF &file = *scene.get();

	fastgltf::Parser parser {};

	constexpr auto gltfOptions = fastgltf::Options::None
		| fastgltf::Options::DontRequireValidAssetMember
		| fastgltf::Options::AllowDouble
		| fastgltf::Options::LoadGLBBuffers
		| fastgltf::Options::LoadExternalBuffers;
	// fastgltf::Options::LoadExternalImages;

	auto result = fastgltf::GltfDataBuffer::FromPath(filePath);
	if (result.error() != fastgltf::Error::None) {
		std::cout << "ERROR: GLTF loading failed (" << magic_enum::enum_name(result.error()) << ") for: " << filePath << '\n';
		return {};
	}

	fastgltf::GltfDataBuffer &data = result.get();

	fastgltf::Asset gltf;

	const std::filesystem::path path = filePath;

	const auto type = fastgltf::determineGltfFileType(data);
	if (type == fastgltf::GltfType::glTF) {
		auto load = parser.loadGltf(data, path.parent_path(), gltfOptions);
		if (load) {
			gltf = std::move(load.get());
		} else {
			std::cerr << "Failed to load glTF: " << fastgltf::to_underlying(load.error()) << '\n';
			return {};
		}
	} else if (type == fastgltf::GltfType::GLB) {
		auto load = parser.loadGltfBinary(data, path.parent_path(), gltfOptions);
		if (load) {
			gltf = std::move(load.get());
		} else {
			std::cerr << "Failed to load glTF: " << fastgltf::to_underlying(load.error()) << '\n';
			return {};
		}
	} else {
		std::cerr << "Failed to determine glTF container" << '\n';
		return {};
	}

	// we can stimate the descriptors we will need accurately
	constexpr DescriptorAllocatorGrowable::PoolSizeRatio sizes[] = {
		{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3},
		{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3},
		{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},
	};

	file.descriptorPool.init(engine._device, gltf.materials.size(), sizes);

	// load samplers
	for (const fastgltf::Sampler &sampler : gltf.samplers) {
		const VkSamplerCreateInfo sampl {
			.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
			.pNext = nullptr,
			.magFilter = extract_filter(sampler.magFilter.value_or(fastgltf::Filter::Nearest)),
			.minFilter = extract_filter(sampler.minFilter.value_or(fastgltf::Filter::Nearest)),
			.mipmapMode = extract_mipmap_mode(sampler.minFilter.value_or(fastgltf::Filter::Nearest)),
			.minLod = 0,
			.maxLod = VK_LOD_CLAMP_NONE,
		};

		VkSampler newSampler = VK_NULL_HANDLE;
		vkCreateSampler(engine._device, &sampl, nullptr, &newSampler);

		file.samplers.push_back(newSampler);
	}

	// temporal arrays for all the objects to use while creating the GLTF data
	std::vector<std::shared_ptr<MeshAsset>> meshes{};
	std::vector<std::shared_ptr<Node>> nodes{};
	std::vector<AllocatedImage> images{};
	std::vector<std::shared_ptr<GLTFMaterial>> materials{};

	// load all textures
	for (const fastgltf::Image &image : gltf.images) {
		images.push_back(engine._errorCheckerboardImage);
	}

	// create buffer to hold the material data
	file.materialDataBuffer = engine.create_buffer(
		sizeof(GLTFMetallic_Roughness::MaterialConstants) * gltf.materials.size(),
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VMA_MEMORY_USAGE_CPU_TO_GPU
	);

	int data_index = 0;
	GLTFMetallic_Roughness::MaterialConstants *sceneMaterialConstants = reinterpret_cast<GLTFMetallic_Roughness::MaterialConstants *>(file.materialDataBuffer.info.pMappedData);

	for (const fastgltf::Material &mat : gltf.materials) {
		std::shared_ptr<GLTFMaterial> newMat = std::make_shared<GLTFMaterial>();
		materials.push_back(newMat);
		file.materials[mat.name.c_str()] = newMat;

		const GLTFMetallic_Roughness::MaterialConstants constants {
			.colorFactors = {
				mat.pbrData.baseColorFactor[0],
				mat.pbrData.baseColorFactor[1],
				mat.pbrData.baseColorFactor[2],
				mat.pbrData.baseColorFactor[3],
			},

			.metal_rough_factors = {
				mat.pbrData.metallicFactor,
				mat.pbrData.roughnessFactor,
				0.f,
				0.f,
			},
		};

		// write material parameters to buffer
		sceneMaterialConstants[data_index] = constants;

		const MaterialPass passType = mat.alphaMode == fastgltf::AlphaMode::Blend
			? MaterialPass::Transparent
			: MaterialPass::MainColor;

		GLTFMetallic_Roughness::MaterialResources materialResources {
			// default the material textures
			.colorImage = engine._whiteImage,
			.colorSampler = engine._defaultSamplerLinear,
			.metalRoughImage = engine._whiteImage,
			.metalRoughSampler = engine._defaultSamplerLinear,

			// set the uniform buffer for the material data
			.dataBuffer = file.materialDataBuffer.buffer,
			.dataBufferOffset = static_cast<uint32_t>(data_index * sizeof(GLTFMetallic_Roughness::MaterialConstants)),
		};

		// grab textures from gltf file
		if (mat.pbrData.baseColorTexture.has_value()) {
			const size_t img = gltf.textures[mat.pbrData.baseColorTexture.value().textureIndex].imageIndex.value();
			const size_t sampler = gltf.textures[mat.pbrData.baseColorTexture.value().textureIndex].samplerIndex.value();

			materialResources.colorImage = images[img];
			materialResources.colorSampler = file.samplers[sampler];
		}

		// build material
		newMat->data = engine.metalRoughMaterial.write_material(engine._device, passType, materialResources, file.descriptorPool);

		data_index++;
	}

	std::vector<uint32_t> indices{};
	std::vector<Vertex> vertices{};

	for (const fastgltf::Mesh &mesh : gltf.meshes) {
		std::shared_ptr<MeshAsset> newmesh = std::make_shared<MeshAsset>();
		meshes.push_back(newmesh);
		file.meshes[mesh.name.c_str()] = newmesh;
		newmesh->name = mesh.name;

		// clear the mesh arrays each mesh, we dont want to merge them by error
		indices.clear();
		vertices.clear();

		for (auto &&p : mesh.primitives) {
			GeoSurface newSurface {
				.startIndex = static_cast<uint32_t>(indices.size()),
				.count = static_cast<uint32_t>(gltf.accessors[p.indicesAccessor.value()].count),
			};

			const size_t initial_vtx = vertices.size();

			// load indexes
			{
				const fastgltf::Accessor &indexaccessor = gltf.accessors[p.indicesAccessor.value()];
				indices.reserve(indices.size() + indexaccessor.count);

				fastgltf::iterateAccessor<std::uint32_t>(gltf, indexaccessor, [&](const std::uint32_t idx) {
					indices.push_back(idx + initial_vtx);
				});
			}

			// load vertex positions
			{
				const fastgltf::Accessor &posAccessor = gltf.accessors[p.findAttribute("POSITION")->accessorIndex];
				vertices.resize(vertices.size() + posAccessor.count);

				fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, posAccessor, [&](const glm::vec3 v, const size_t index) {
					const Vertex newvtx {
						.position = v,
						.uv_x = 0,
						.normal = { 1, 0, 0 },
						.uv_y = 0,
						.color = glm::vec4 { 1.f },
					};

					vertices[initial_vtx + index] = newvtx;
				});
			}

			// load vertex normals
			const auto normals = p.findAttribute("NORMAL");
			if (normals != p.attributes.end()) {
				fastgltf::iterateAccessorWithIndex<glm::vec3>(gltf, gltf.accessors[(*normals).accessorIndex], [&](const glm::vec3 v, const size_t index) {
					vertices[initial_vtx + index].normal = v;
				});
			}

			// load UVs
			const auto uv = p.findAttribute("TEXCOORD_0");
			if (uv != p.attributes.end()) {
				fastgltf::iterateAccessorWithIndex<glm::vec2>(gltf, gltf.accessors[(*uv).accessorIndex], [&](const glm::vec2 v, const size_t index) {
					vertices[initial_vtx + index].uv_x = v.x;
					vertices[initial_vtx + index].uv_y = v.y;
				});
			}

			// load vertex colors
			const auto colors = p.findAttribute("COLOR_0");
			if (colors != p.attributes.end()) {
				fastgltf::iterateAccessorWithIndex<glm::vec4>(gltf, gltf.accessors[(*colors).accessorIndex], [&](const glm::vec4 v, const size_t index) {
					vertices[initial_vtx + index].color = v;
				});
			}

			if (p.materialIndex.has_value()) {
				newSurface.material = materials[p.materialIndex.value()];
			} else {
				newSurface.material = materials[0];
			}

			//loop the vertices of this surface, find min/max bounds
			glm::vec3 minpos = vertices[initial_vtx].position;
			glm::vec3 maxpos = vertices[initial_vtx].position;

			for (int i = initial_vtx; i < vertices.size(); i++) {
				minpos = glm::min(minpos, vertices[i].position);
			}
			for (int i = initial_vtx; i < vertices.size(); i++) {
				maxpos = glm::max(maxpos, vertices[i].position);
			}

			// calculate origin and extents from the min/max, use extent lenght for radius
			newSurface.bounds.origin = (maxpos + minpos) / 2.f;
			newSurface.bounds.extents = (maxpos - minpos) / 2.f;
			newSurface.bounds.sphereRadius = glm::length(newSurface.bounds.extents);

			newmesh->surfaces.push_back(newSurface);
		}

		newmesh->meshBuffers = engine.uploadMesh(indices, vertices);
	}

	// load all nodes and their meshes
	for (const fastgltf::Node &node : gltf.nodes) {
		std::shared_ptr<Node> newNode = node.meshIndex.has_value()
			? std::make_shared<MeshNode>()
			: std::make_shared<Node>();

		// find if the node has a mesh, and if it does hook it to the mesh pointer and allocate it with the meshnode class
		if (node.meshIndex.has_value()) {
			static_cast<MeshNode*>(newNode.get())->mesh = meshes[*node.meshIndex];
		}

		nodes.push_back(newNode);
		file.nodes[node.name.c_str()];

		std::visit(
			fastgltf::visitor {
				[&](const fastgltf::math::fmat4x4 matrix) {
					std::memcpy(&newNode->localTransform, matrix.data(), sizeof(matrix));
				},
				[&](const fastgltf::TRS transform) {
					const glm::vec3 tl(transform.translation[0], transform.translation[1], transform.translation[2]);
					const glm::quat rot(transform.rotation[3], transform.rotation[0], transform.rotation[1], transform.rotation[2]);
					const glm::vec3 sc(transform.scale[0], transform.scale[1], transform.scale[2]);

					const glm::mat4 tm = glm::translate(glm::mat4(1.f), tl);
					const glm::mat4 rm = glm::toMat4(rot);
					const glm::mat4 sm = glm::scale(glm::mat4(1.f), sc);

					newNode->localTransform = tm * rm * sm;
				},
			},
			node.transform
		);
	}

	// run loop again to setup transform hierarchy
	for (int i = 0; i < gltf.nodes.size(); i++) {
		const fastgltf::Node &node = gltf.nodes[i];
		std::shared_ptr<Node> &sceneNode = nodes[i];

		for (const auto &c : node.children) {
			sceneNode->children.push_back(nodes[c]);
			nodes[c]->parent = sceneNode;
		}
	}

	// find the top nodes, with no parents
	for (auto &node : nodes) {
		if (node->parent.lock() == nullptr) {
			file.topNodes.push_back(node);
			node->refreshTransform(glm::mat4 {1.f});
		}
	}

	// load all textures
	for (fastgltf::Image &image : gltf.images) {
		const std::optional<AllocatedImage> img = load_image(engine, gltf, image);

		if (img.has_value()) {
			images.push_back(*img);
			file.images[image.name.c_str()] = *img;
		} else {
			// we failed to load, so lets give the slot a default white texture to not
			// completely break loading
			images.push_back(engine._errorCheckerboardImage);
			std::cout << "gltf failed to load texture " << image.name << '\n';
		}
	}

	return scene;
}

std::optional<AllocatedImage> load_image(Engine &engine, fastgltf::Asset &asset, fastgltf::Image &image)
{
	AllocatedImage newImage {};

	int width, height, nrChannels;

	std::visit(
		fastgltf::visitor {
			[](const auto &arg) {},
			[&](const fastgltf::sources::URI &filePath) {
				assert(filePath.fileByteOffset == 0); // We don't support offsets with stbi.
				assert(filePath.uri.isLocalPath()); // We're only capable of loading
				// local files.

				const std::string path(filePath.uri.path().begin(), filePath.uri.path().end()); // Thanks C++.
				unsigned char *data = stbi_load(path.c_str(), &width, &height, &nrChannels, 4);
				if (data) {
					const VkExtent3D imagesize {
						.width = static_cast<uint32_t>(width),
						.height = static_cast<uint32_t>(height),
						.depth = 1,
					};

					newImage = engine.create_image(data, imagesize, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, false);

					stbi_image_free(data);
				}
			},
			[&](const fastgltf::sources::Vector &vector) {
				unsigned char *data = stbi_load_from_memory(sweep<stbi_uc>(vector.bytes.data()), static_cast<int>(vector.bytes.size()), &width, &height, &nrChannels, 4);
				if (data) {
					const VkExtent3D imagesize {
						.width = static_cast<uint32_t>(width),
						.height = static_cast<uint32_t>(height),
						.depth = 1,
					};

					newImage = engine.create_image(data, imagesize, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, false);

					stbi_image_free(data);
				}
			},
			[&](const fastgltf::sources::BufferView &view) {
				const auto &bufferView = asset.bufferViews[view.bufferViewIndex];
				const auto &buffer = asset.buffers[bufferView.bufferIndex];

				std::visit(fastgltf::visitor { // We only care about VectorWithMime here, because we
					// specify LoadExternalBuffers, meaning all buffers
					// are already loaded into a vector.
					[](const auto &arg) {},
					[&](const fastgltf::sources::Vector &vector) {
					unsigned char *data = stbi_load_from_memory(sweep<stbi_uc>(vector.bytes.data() + bufferView.byteOffset), static_cast<int>(bufferView.byteLength), &width, &height, &nrChannels, 4);
					if (data) {
						const VkExtent3D imagesize {
							.width = static_cast<uint32_t>(width),
							.height = static_cast<uint32_t>(height),
							.depth = 1,
						};

						newImage = engine.create_image(data, imagesize, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, false);

						stbi_image_free(data);
					}
				}}, buffer.data);
			},
		},
		image.data
	);

	// if any of the attempts to load the data failed, we havent written the image
	// so handle is null
	if (newImage.image == VK_NULL_HANDLE) {
		return {};
	}

	return newImage;
}

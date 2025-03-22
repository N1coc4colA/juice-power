#ifndef LOADER_H
#define LOADER_H

#include <vector>
#include <string>
#include <optional>
#include <memory>
#include <filesystem>

#include <fastgltf/types.hpp>

#include "types.h"
#include "material.h"


class Engine;
class LoadedGLTF;


struct GLTFMaterial
{
	MaterialInstance data;
};

struct Bounds {
	glm::vec3 origin = {};
	float sphereRadius = 0.f;
	glm::vec3 extents = {};
};

struct GeoSurface
{
	uint32_t startIndex = -1;
	uint32_t count = -1;
	Bounds bounds = {};
	std::shared_ptr<GLTFMaterial> material = {};
};

struct MeshAsset
{
	std::string name = "";

	std::vector<GeoSurface> surfaces = {};
	GPUMeshBuffers meshBuffers = {};
};


std::optional<std::vector<std::shared_ptr<MeshAsset>>> loadGltfMeshes(Engine &engine, const std::filesystem::path &filePath);
std::optional<std::shared_ptr<LoadedGLTF>> loadGltf(Engine &engine, const std::filesystem::path &filePath);
std::optional<AllocatedImage> load_image(Engine &engine, fastgltf::Asset &asset, fastgltf::Image &image);


#endif // LOADER_H

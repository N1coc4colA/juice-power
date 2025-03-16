#ifndef LOADER_H
#define LOADER_H

#include <vector>
#include <string>
#include <optional>
#include <memory>
#include <filesystem>

#include "types.h"
#include "material.h"


class Engine;


struct GLTFMaterial
{
	MaterialInstance data;
};

struct GeoSurface
{
	uint32_t startIndex = -1;
	uint32_t count = -1;
	std::shared_ptr<GLTFMaterial> material = {};
};

struct MeshAsset
{
	std::string name = "";

	std::vector<GeoSurface> surfaces = {};
	GPUMeshBuffers meshBuffers = {};
};


std::optional<std::vector<std::shared_ptr<MeshAsset>>> loadGltfMeshes(Engine &engine, const std::filesystem::path &filePath);


#endif // LOADER_H

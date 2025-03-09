#ifndef LOADER_H
#define LOADER_H

#include <vector>
#include <string>
#include <optional>
#include <memory>
#include <filesystem>

#include "types.h"


struct GeoSurface
{
	uint32_t startIndex;
	uint32_t count;
};

struct MeshAsset
{
	std::string name;

	std::vector<GeoSurface> surfaces;
	GPUMeshBuffers meshBuffers;
};

class Engine;


std::optional<std::vector<std::shared_ptr<MeshAsset>>> loadGltfMeshes(Engine *engine, const std::filesystem::path &filePath);


#endif // LOADER_H

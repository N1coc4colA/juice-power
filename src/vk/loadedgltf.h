#ifndef LOADEDGLTF_H
#define LOADEDGLTF_H

#include <vector>
#include <unordered_map>
#include <string>
#include <memory>

#include "renderable.h"
#include "descriptors.h"
#include "types.h"


class Engine;
class Node;
struct AllocatedImage;
struct GLTFMaterial;
struct MeshAsset;


class LoadedGLTF
	: public Renderable
{
public:
	// storage for all the data on a given glTF file
	std::unordered_map<std::string, std::shared_ptr<MeshAsset>> meshes;
	std::unordered_map<std::string, std::shared_ptr<Node>> nodes;
	std::unordered_map<std::string, AllocatedImage> images;
	std::unordered_map<std::string, std::shared_ptr<GLTFMaterial>> materials;

	// nodes that dont have a parent, for iterating through the file in tree order
	std::vector<std::shared_ptr<Node>> topNodes;

	std::vector<VkSampler> samplers;

	DescriptorAllocatorGrowable descriptorPool;

	AllocatedBuffer materialDataBuffer;

	Engine *creator;

	inline ~LoadedGLTF() { clearAll(); };

	void draw(const glm::mat4 &topMatrix, DrawContext &ctx) override;

private:
	void clearAll();
};

#endif // LOADEDGLTF_H

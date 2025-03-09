#ifndef MATERIAL_H
#define MATERIAL_H

#include <vulkan/vulkan_core.h>

#include <glm/glm.hpp>

#include "allocatedimage.h"
#include "descriptors.h"


class Engine;


struct MaterialPipeline
{
	VkPipeline pipeline;
	VkPipelineLayout layout;
};

enum class MaterialPass : uint8_t
{
	MainColor,
	Transparent,
	Other,

	First = MainColor,
	Last = Other,
};

struct MaterialInstance
{
	MaterialPipeline *pipeline;
	VkDescriptorSet materialSet;
	MaterialPass passType;
};

struct GLTFMetallic_Roughness
{
	MaterialPipeline opaquePipeline;
	MaterialPipeline transparentPipeline;

	VkDescriptorSetLayout materialLayout;

	struct MaterialConstants {
		glm::vec4 colorFactors;
		glm::vec4 metal_rough_factors;
		//padding, we need it anyway for uniform buffers
		glm::vec4 extra[14];
	};

	struct MaterialResources {
		AllocatedImage colorImage;
		VkSampler colorSampler;
		AllocatedImage metalRoughImage;
		VkSampler metalRoughSampler;
		VkBuffer dataBuffer;
		uint32_t dataBufferOffset;
	};

	DescriptorWriter writer;

	void build_pipelines(Engine *engine);
	void clear_resources(VkDevice device);

	MaterialInstance write_material(VkDevice device, const MaterialPass pass, const MaterialResources &resources, DescriptorAllocatorGrowable &descriptorAllocator);
};


#endif // MATERIAL_H

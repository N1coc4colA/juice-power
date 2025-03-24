#ifndef MATERIAL_H
#define MATERIAL_H

#include <vulkan/vulkan_core.h>

#include <glm/glm.hpp>

#include "allocatedimage.h"
#include "descriptors.h"


class Engine;


struct MaterialPipeline
{
	VkPipeline pipeline = VK_NULL_HANDLE;
	VkPipelineLayout layout = VK_NULL_HANDLE;
};

enum class MaterialPass : uint8_t
{
	Invalid,
	MainColor,
	Transparent,
	Other,

	First = Invalid,
	Last = Other,
};

struct MaterialInstance
{
	MaterialPipeline *pipeline = nullptr;
	VkDescriptorSet materialSet = VK_NULL_HANDLE;
	MaterialPass passType = MaterialPass::Invalid;
};

struct GLTFMetallic_Roughness
{
	MaterialPipeline opaquePipeline {};
	MaterialPipeline transparentPipeline {};

	VkDescriptorSetLayout materialLayout = VK_NULL_HANDLE;

	struct MaterialConstants {
		glm::vec4 colorFactors {};
		glm::vec4 metal_rough_factors {};
		//padding, we need it anyway for uniform buffers
		glm::vec4 extra[14];
	};

	struct MaterialResources {
		AllocatedImage colorImage {};
		VkSampler colorSampler = VK_NULL_HANDLE;
		AllocatedImage metalRoughImage {};
		VkSampler metalRoughSampler = VK_NULL_HANDLE;
		VkBuffer dataBuffer = VK_NULL_HANDLE;
		uint32_t dataBufferOffset = -1;
	};

	DescriptorWriter writer {};

	void build_pipelines(Engine &engine);
	void clear_resources(VkDevice device);

	MaterialInstance write_material(VkDevice device, const MaterialPass pass, const MaterialResources &resources, DescriptorAllocatorGrowable &descriptorAllocator);
};


#endif // MATERIAL_H

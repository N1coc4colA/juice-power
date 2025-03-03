#ifndef TYPES_H
#define TYPES_H

#include <vulkan/vulkan.h>

#include <glm/glm.hpp>

#include "vma.h"


struct AllocatedImage
{
	VkImage image = VK_NULL_HANDLE;
	VkImageView imageView = VK_NULL_HANDLE;
	VmaAllocation allocation = VK_NULL_HANDLE;
	VkExtent3D imageExtent;
	VkFormat imageFormat;
};

struct MeshPushConstants
{
	glm::vec4 data;
	glm::mat4 render_matrix;
};

struct ComputePushConstants
{
	glm::vec4 data1;
	glm::vec4 data2;
	glm::vec4 data3;
	glm::vec4 data4;
};

struct ComputeEffect
{
	const char *name;

	VkPipeline pipeline;
	VkPipelineLayout layout;

	ComputePushConstants data;
};


#endif // TYPES_H

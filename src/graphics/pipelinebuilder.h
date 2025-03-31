#ifndef PIPELINEBUILDER_H
#define PIPELINEBUILDER_H

#include <vector>

#include <vulkan/vulkan.h>


class PipelineBuilder
{
public:
	std::vector<VkPipelineShaderStageCreateInfo> shaderStages {};

	VkPipelineInputAssemblyStateCreateInfo inputAssembly {.sType = VK_STRUCTURE_TYPE_MAX_ENUM};
	VkPipelineRasterizationStateCreateInfo rasterizer {.sType = VK_STRUCTURE_TYPE_MAX_ENUM};
	VkPipelineColorBlendAttachmentState colorBlendAttachment {};
	VkPipelineMultisampleStateCreateInfo multisampling {.sType = VK_STRUCTURE_TYPE_MAX_ENUM};
	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
	VkPipelineDepthStencilStateCreateInfo depthStencil {.sType = VK_STRUCTURE_TYPE_MAX_ENUM};
	VkPipelineRenderingCreateInfo renderInfo {.sType = VK_STRUCTURE_TYPE_MAX_ENUM};
	VkFormat colorAttachmentformat = VK_FORMAT_MAX_ENUM;

	inline PipelineBuilder()
	{
		clear();
	}

	void clear();

	VkPipeline buildPipeline(VkDevice device);

	void setShaders(VkShaderModule vertexShader, VkShaderModule fragmentShader);
	void setInputTopology(const VkPrimitiveTopology topology);
	void setPolygonMode(const VkPolygonMode mode);
	void setCullMode(const VkCullModeFlags cullMode, const VkFrontFace frontFace);
	void setMultisamplingNone();
	void disableBlending();
	void setColorAttachmentFormat(const VkFormat format);
	void setDepthFormat(const VkFormat format);
	void disableDepthtest();
	void enableDepthtest(bool depthWriteEnable, VkCompareOp op);
	void enableBlendingAdditive();
	void enableBlendingAlphablend();
};


#endif // PIPELINEBUILDER_H

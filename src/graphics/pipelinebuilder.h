#ifndef PIPELINEBUILDER_H
#define PIPELINEBUILDER_H

#include <vector>

#include <vulkan/vulkan.h>


class PipelineBuilder
{
public:
	std::vector<VkPipelineShaderStageCreateInfo> _shaderStages;

	VkPipelineInputAssemblyStateCreateInfo _inputAssembly {.sType = VK_STRUCTURE_TYPE_MAX_ENUM};
	VkPipelineRasterizationStateCreateInfo _rasterizer {.sType = VK_STRUCTURE_TYPE_MAX_ENUM};
	VkPipelineColorBlendAttachmentState _colorBlendAttachment {};
	VkPipelineMultisampleStateCreateInfo _multisampling {.sType = VK_STRUCTURE_TYPE_MAX_ENUM};
	VkPipelineLayout _pipelineLayout = VK_NULL_HANDLE;
	VkPipelineDepthStencilStateCreateInfo _depthStencil {.sType = VK_STRUCTURE_TYPE_MAX_ENUM};
	VkPipelineRenderingCreateInfo _renderInfo {.sType = VK_STRUCTURE_TYPE_MAX_ENUM};
	VkFormat _colorAttachmentformat = VK_FORMAT_MAX_ENUM;

	inline PipelineBuilder()
	{
		clear();
	}

	void clear();

	VkPipeline build_pipeline(VkDevice device);

	void set_shaders(VkShaderModule vertexShader, VkShaderModule fragmentShader);
	void set_input_topology(const VkPrimitiveTopology topology);
	void set_polygon_mode(const VkPolygonMode mode);
	void set_cull_mode(const VkCullModeFlags cullMode, const VkFrontFace frontFace);
	void set_multisampling_none();
	void disable_blending();
	void set_color_attachment_format(const VkFormat format);
	void set_depth_format(const VkFormat format);
	void disable_depthtest();
	void enable_depthtest(bool depthWriteEnable, VkCompareOp op);
	void enable_blending_additive();
	void enable_blending_alphablend();
};


#endif // PIPELINEBUILDER_H

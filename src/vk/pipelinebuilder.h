#ifndef PIPELINEBUILDER_H
#define PIPELINEBUILDER_H

#include <vector>

#include <vulkan/vulkan.h>


class PipelineBuilder
{
public:
	std::vector<VkPipelineShaderStageCreateInfo> _shaderStages;

	VkPipelineInputAssemblyStateCreateInfo _inputAssembly;
	VkPipelineRasterizationStateCreateInfo _rasterizer;
	VkPipelineColorBlendAttachmentState _colorBlendAttachment;
	VkPipelineMultisampleStateCreateInfo _multisampling;
	VkPipelineLayout _pipelineLayout;
	VkPipelineDepthStencilStateCreateInfo _depthStencil;
	VkPipelineRenderingCreateInfo _renderInfo;
	VkFormat _colorAttachmentformat;

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
};

#endif // PIPELINEBUILDER_H

#include "pipelinebuilder.h"

#include <cassert>

#include <fmt/printf.h>

#include "initializers.h"


void PipelineBuilder::clear()
{
	// clear all of the structs we need back to 0 with their correct stype

	_inputAssembly = { .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };

	_rasterizer = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };

	_colorBlendAttachment = {};

	_multisampling = { .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };

	_pipelineLayout = {};

	_depthStencil = { .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };

	_renderInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };

	_shaderStages.clear();
}

VkPipeline PipelineBuilder::build_pipeline(VkDevice device)
{
	assert(device != VK_NULL_HANDLE);

	// make viewport state from our stored viewport and scissor.
	// at the moment we wont support multiple viewports or scissors
	const VkPipelineViewportStateCreateInfo viewportState = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.pNext = nullptr,
		.viewportCount = 1,
		.scissorCount = 1,
	};

	// setup dummy color blending. We arent using transparent objects yet
	// the blending is just "no blend", but we do write to the color attachment
	const VkPipelineColorBlendStateCreateInfo colorBlending = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.pNext = nullptr,

		.logicOpEnable = VK_FALSE,
		.logicOp = VK_LOGIC_OP_COPY,
		.attachmentCount = 1,
		.pAttachments = &_colorBlendAttachment,
	};

	// completely clear VertexInputStateCreateInfo, as we have no need for it
	const VkPipelineVertexInputStateCreateInfo _vertexInputInfo {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO
	};

	// build the actual pipeline
	const VkDynamicState state[] = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR,
	};

	const VkPipelineDynamicStateCreateInfo dynamicInfo {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		.dynamicStateCount = 2,
		.pDynamicStates = &state[0],
	};

	// we now use all of the info structs we have been writing into into this one
	// to create the pipeline
	const VkGraphicsPipelineCreateInfo pipelineInfo {
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		// connect the renderInfo to the pNext extension mechanism
		.pNext = &_renderInfo,

		.stageCount = (uint32_t)_shaderStages.size(),
		.pStages = _shaderStages.data(),
		.pVertexInputState = &_vertexInputInfo,
		.pInputAssemblyState = &_inputAssembly,
		.pViewportState = &viewportState,
		.pRasterizationState = &_rasterizer,
		.pMultisampleState = &_multisampling,
		.pDepthStencilState = &_depthStencil,
		.pColorBlendState = &colorBlending,
		.pDynamicState = &dynamicInfo,
		.layout = _pipelineLayout,
	};

	// its easy to error out on create graphics pipeline, so we handle it a bit
	// better than the common VK_CHECK case
	VkPipeline newPipeline = VK_NULL_HANDLE;
	if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &newPipeline) != VK_SUCCESS) {
		fmt::println("failed to create pipeline");
		return VK_NULL_HANDLE; // failed to create graphics pipeline
	} else {
		return newPipeline;
	}
}

void PipelineBuilder::set_shaders(VkShaderModule vertexShader, VkShaderModule fragmentShader)
{
	assert(vertexShader != VK_NULL_HANDLE);
	assert(fragmentShader != VK_NULL_HANDLE);

	_shaderStages.clear();

	_shaderStages.push_back(vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, vertexShader));

	_shaderStages.push_back(vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, fragmentShader));
}

void PipelineBuilder::set_input_topology(const VkPrimitiveTopology topology)
{
	assert(topology != VK_PRIMITIVE_TOPOLOGY_MAX_ENUM);

	_inputAssembly.topology = topology;
	// we are not going to use primitive restart on the entire tutorial so leave
	// it on false
	_inputAssembly.primitiveRestartEnable = VK_FALSE;
}

void PipelineBuilder::set_polygon_mode(const VkPolygonMode mode)
{
	assert(mode != VK_POLYGON_MODE_MAX_ENUM);

	_rasterizer.polygonMode = mode;
	_rasterizer.lineWidth = 1.f;
}

void PipelineBuilder::set_cull_mode(VkCullModeFlags cullMode, VkFrontFace frontFace)
{
	assert(cullMode != VK_CULL_MODE_FLAG_BITS_MAX_ENUM);
	assert(frontFace != VK_FRONT_FACE_MAX_ENUM);

	_rasterizer.cullMode = cullMode;
	_rasterizer.frontFace = frontFace;
}

void PipelineBuilder::set_multisampling_none()
{
	_multisampling.sampleShadingEnable = VK_FALSE;
	// multisampling defaulted to no multisampling (1 sample per pixel)
	_multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	_multisampling.minSampleShading = 1.0f;
	_multisampling.pSampleMask = nullptr;
	// no alpha to coverage either
	_multisampling.alphaToCoverageEnable = VK_FALSE;
	_multisampling.alphaToOneEnable = VK_FALSE;
}

void PipelineBuilder::disable_blending()
{
	// default write mask
	_colorBlendAttachment.colorWriteMask = 0
		| VK_COLOR_COMPONENT_R_BIT
		| VK_COLOR_COMPONENT_G_BIT
		| VK_COLOR_COMPONENT_B_BIT
		| VK_COLOR_COMPONENT_A_BIT;
	// no blending
	_colorBlendAttachment.blendEnable = VK_FALSE;
}

void PipelineBuilder::set_color_attachment_format(const VkFormat format)
{
	assert(format != VK_FORMAT_MAX_ENUM);

	_colorAttachmentformat = format;
	// connect the format to the renderInfo  structure
	_renderInfo.colorAttachmentCount = 1;
	_renderInfo.pColorAttachmentFormats = &_colorAttachmentformat;
}

void PipelineBuilder::set_depth_format(const VkFormat format)
{
	assert(format != VK_FORMAT_MAX_ENUM);

	_renderInfo.depthAttachmentFormat = format;
}

void PipelineBuilder::disable_depthtest()
{
	_depthStencil.depthTestEnable = VK_FALSE;
	_depthStencil.depthWriteEnable = VK_FALSE;
	_depthStencil.depthCompareOp = VK_COMPARE_OP_NEVER;
	_depthStencil.depthBoundsTestEnable = VK_FALSE;
	_depthStencil.stencilTestEnable = VK_FALSE;
	_depthStencil.front = {};
	_depthStencil.back = {};
	_depthStencil.minDepthBounds = 0.f;
	_depthStencil.maxDepthBounds = 1.f;
}

void PipelineBuilder::enable_depthtest(bool depthWriteEnable, VkCompareOp op)
{
	_depthStencil.depthTestEnable = VK_TRUE;
	_depthStencil.depthWriteEnable = depthWriteEnable;
	_depthStencil.depthCompareOp = op;
	_depthStencil.depthBoundsTestEnable = VK_FALSE;
	_depthStencil.stencilTestEnable = VK_FALSE;
	_depthStencil.front = {};
	_depthStencil.back = {};
	_depthStencil.minDepthBounds = 0.f;
	_depthStencil.maxDepthBounds = 1.f;
}

void PipelineBuilder::enable_blending_additive()
{
	_colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	_colorBlendAttachment.blendEnable = VK_TRUE;
	_colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	_colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
	_colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	_colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	_colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	_colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
}

void PipelineBuilder::enable_blending_alphablend()
{
	_colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	_colorBlendAttachment.blendEnable = VK_TRUE;
	_colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	_colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	_colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	_colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	_colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	_colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
}

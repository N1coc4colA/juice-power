#include "material.h"

#include <fmt/printf.h>

#include "defines.h"
#include "engine.h"
#include "types.h"
#include "pipelinebuilder.h"
#include "initializers.h"
#include "utils.h"

#include "../graphics/engine.h"


void GLTFMetallic_Roughness::buildPipelines(Graphics::Engine &engine)
{
	VkShaderModule meshFragShader = VK_NULL_HANDLE;
	if (!vkutil::load_shader_module(COMPILED_SHADERS_DIR "/mesh.frag.spv", engine.device, meshFragShader)) {
		fmt::println("Error when building the triangle fragment shader module\n");
	}

	VkShaderModule meshVertexShader = VK_NULL_HANDLE;
	if (!vkutil::load_shader_module(COMPILED_SHADERS_DIR "/mesh.vert.spv", engine.device, meshVertexShader)) {
		fmt::println("Error when building the triangle vertex shader module\n");
	}

	const VkPushConstantRange matrixRange {
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
		.offset = 0,
		.size = sizeof(GPUDrawPushConstants),
	};

	DescriptorLayoutBuilder layoutBuilder;
	layoutBuilder.addBinding(0,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	layoutBuilder.addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	layoutBuilder.addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

	materialLayout = layoutBuilder.build(engine.device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

	const VkDescriptorSetLayout layouts[] = {
		engine.gpuSceneDataDescriptorLayout,
		materialLayout
	};

	VkPipelineLayoutCreateInfo mesh_layout_info = vkinit::pipeline_layout_create_info();
	mesh_layout_info.setLayoutCount = 2;
	mesh_layout_info.pSetLayouts = layouts;
	mesh_layout_info.pPushConstantRanges = &matrixRange;
	mesh_layout_info.pushConstantRangeCount = 1;

	VkPipelineLayout newLayout = VK_NULL_HANDLE;
	VK_CHECK(vkCreatePipelineLayout(engine.device, &mesh_layout_info, nullptr, &newLayout));

	opaquePipeline.layout = newLayout;
	transparentPipeline.layout = newLayout;
}


void GLTFMetallic_Roughness::clearResources(VkDevice device)
{
	assert(device != VK_NULL_HANDLE);
}

MaterialInstance GLTFMetallic_Roughness::writeMaterial(VkDevice device, const MaterialPass pass, const MaterialResources &resources, DescriptorAllocatorGrowable &descriptorAllocator)
{
	assert(device != VK_NULL_HANDLE);
	assert(pass != MaterialPass::Invalid);

	const MaterialInstance matData {
		.pipeline = pass == MaterialPass::Transparent
			? &transparentPipeline
			: &opaquePipeline,
		.materialSet = descriptorAllocator.allocate(device, materialLayout),
		.passType = pass,
	};

	writer.clear();
	writer.writeBuffer(0, resources.dataBuffer, sizeof(MaterialConstants), resources.dataBufferOffset, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	writer.writeImage(1, resources.colorImage.imageView, resources.colorSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	writer.writeImage(2, resources.metalRoughImage.imageView, resources.metalRoughSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

	writer.updateSet(device, matData.materialSet);

	return matData;
}

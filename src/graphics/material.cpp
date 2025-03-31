#include "material.h"

#include <fmt/printf.h>

#include "defines.h"
#include "engine.h"
#include "types.h"
#include "pipelinebuilder.h"
#include "initializers.h"
#include "utils.h"

#include "../graphics/engine.h"



	/*


	// build the stage-create-info for both vertex and fragment stages. This lets
	// the pipeline know the shader modules per stage
	PipelineBuilder pipelineBuilder;
	pipelineBuilder.set_shaders(meshVertexShader, meshFragShader);
	pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	pipelineBuilder.set_polygon_mode(VK_POLYGON_MODE_FILL);
	pipelineBuilder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
	pipelineBuilder.set_multisampling_none();
	pipelineBuilder.disable_blending();
	pipelineBuilder.enable_depthtest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);

	//render format
	pipelineBuilder.set_color_attachment_format(engine._drawImage.imageFormat);
	pipelineBuilder.set_depth_format(engine._depthImage.imageFormat);

	// use the triangle layout we created
	pipelineBuilder._pipelineLayout = newLayout;

	// finally build the pipeline
	opaquePipeline.pipeline = pipelineBuilder.build_pipeline(engine._device);

	// create the transparent variant
	pipelineBuilder.enable_blending_additive();

	pipelineBuilder.enable_depthtest(false, VK_COMPARE_OP_GREATER_OR_EQUAL);

	transparentPipeline.pipeline = pipelineBuilder.build_pipeline(engine._device);

	vkDestroyShaderModule(engine._device, meshFragShader, nullptr);
	vkDestroyShaderModule(engine._device, meshVertexShader, nullptr);


	*/

void GLTFMetallic_Roughness::build_pipelines(Graphics::Engine &engine)
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
	layoutBuilder.add_binding(0,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	layoutBuilder.add_binding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	layoutBuilder.add_binding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

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


void GLTFMetallic_Roughness::clear_resources(VkDevice device)
{
	assert(device != VK_NULL_HANDLE);
}

MaterialInstance GLTFMetallic_Roughness::write_material(VkDevice device, const MaterialPass pass, const MaterialResources &resources, DescriptorAllocatorGrowable &descriptorAllocator)
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
	writer.write_buffer(0, resources.dataBuffer, sizeof(MaterialConstants), resources.dataBufferOffset, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	writer.write_image(1, resources.colorImage.imageView, resources.colorSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
	writer.write_image(2, resources.metalRoughImage.imageView, resources.metalRoughSampler, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

	writer.update_set(device, matData.materialSet);

	return matData;
}

#ifndef PIPELINEBUILDER_H
#define PIPELINEBUILDER_H

#include <vector>

#include <vulkan/vulkan.h>


namespace Graphics
{

/**
 * @brief Builder class for constructing Vulkan graphics pipelines
 *
 * Provides a fluent interface for configuring and creating VkPipeline objects.
 * Manages pipeline state configuration including shaders, vertex input,
 * rasterization, blending, and multisampling.
 */
class PipelineBuilder
{
public:
	/// @brief Shader stages to include in the pipeline
	std::vector<VkPipelineShaderStageCreateInfo> shaderStages {};

	/// @brief Vertex input assembly state (topology)
	VkPipelineInputAssemblyStateCreateInfo inputAssembly {.sType = VK_STRUCTURE_TYPE_MAX_ENUM};

	/// @brief Rasterization state (polygon mode, culling, etc)
	VkPipelineRasterizationStateCreateInfo rasterizer {.sType = VK_STRUCTURE_TYPE_MAX_ENUM};

	/// @brief Color blending attachment state
	VkPipelineColorBlendAttachmentState colorBlendAttachment {};

	/// @brief Multisampling state
	VkPipelineMultisampleStateCreateInfo multisampling {.sType = VK_STRUCTURE_TYPE_MAX_ENUM};

	/// @brief Pipeline layout
	VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;

	/// @brief Depth/stencil testing state
	VkPipelineDepthStencilStateCreateInfo depthStencil {.sType = VK_STRUCTURE_TYPE_MAX_ENUM};

	/// @brief Dynamic rendering info (for VK_KHR_dynamic_rendering)
	VkPipelineRenderingCreateInfo renderInfo {.sType = VK_STRUCTURE_TYPE_MAX_ENUM};

	/// @brief Color attachment format
	VkFormat colorAttachmentformat = VK_FORMAT_MAX_ENUM;

	/// @brief Constructs a new PipelineBuilder with default states
	inline PipelineBuilder()
	{
		clear();
	}

	/// @brief Resets all pipeline states to defaults
	void clear();

	/**
	 * @brief Builds the finalized pipeline object
	 * @param device The Vulkan logical device
	 * @return The created VkPipeline object
	 * @throws Failure with VkPipelineCreation if creation fails
	 */
	VkPipeline buildPipeline(VkDevice device);

	/**
	 * @brief Sets the vertex and fragment shaders
	 * @param vertexShader Module containing vertex shader code
	 * @param fragmentShader Module containing fragment shader code
	 */
	void setShaders(VkShaderModule vertexShader, VkShaderModule fragmentShader);

	/**
	 * @brief Sets the primitive topology
	 * @param topology The primitive topology (e.g. VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
	 */
	void setInputTopology(const VkPrimitiveTopology topology);

	/**
	 * @brief Sets the polygon rasterization mode
	 * @param mode The polygon mode (e.g. VK_POLYGON_MODE_FILL)
	 */
	void setPolygonMode(const VkPolygonMode mode);

	/**
	 * @brief Configures face culling
	 * @param cullMode Which faces to cull (e.g. VK_CULL_MODE_BACK_BIT)
	 * @param frontFace Winding order for front faces
	 */
	void setCullMode(const VkCullModeFlags cullMode, const VkFrontFace frontFace);

	/// @brief Disables multisampling
	void setMultisamplingNone();

	/// @brief Disables color blending
	void disableBlending();

	/**
	 * @brief Sets the color attachment format
	 * @param format The color attachment format
	 */
	void setColorAttachmentFormat(const VkFormat format);

	/**
	 * @brief Sets the depth attachment format
	 * @param format The depth attachment format
	 */
	void setDepthFormat(const VkFormat format);

	/// @brief Disables depth testing
	void disableDepthtest();

	/**
	 * @brief Enables depth testing
	 * @param depthWriteEnable Whether to write depth values
	 * @param op The depth comparison operator
	 */
	void enableDepthtest(bool depthWriteEnable, VkCompareOp op);

	/// @brief Enables additive blending (src + dst)
	void enableBlendingAdditive();

	/// @brief Enables alpha blending (src.alpha * src.rgb + (1-src.alpha) * dst.rgb)
	void enableBlendingAlphablend();
};


}


#endif // PIPELINEBUILDER_H

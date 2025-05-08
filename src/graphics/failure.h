#ifndef GRAPHICS_ERROR_H
#define GRAPHICS_ERROR_H

#include <cstdint>
#include <exception>
#include <string>


namespace Graphics
{

/**
 * @brief Enumeration of possible failure types in a Vulkan graphics engine
 *
 * Categorizes all known failure points during engine initialization and operation,
 * with specific attention to Vulkan, SDL, and ImGui integration points.
 */
enum FailureType : uint8_t
{
	// ImGui related failures
	ImguiContext = 0,           ///< Failed to create ImGui context
	ImguiInitialisation,        ///< General ImGui initialization failure
	ImguiFontsInitialisation,   ///< Failed to load ImGui font atlas
	ImguiVkInitialisation,      ///< Failed to initialize ImGui Vulkan backend

	// SDL related failures
	SDLInitialisation,          ///< SDL library initialization failed
	SDLWindowCreation,          ///< SDL window creation failed
	SDLVkSurfaceCreation,       ///< Failed to create Vulkan surface from SDL window

	// Vulkan resource creation failures
	VkBufferAllocation,         ///< Failed to allocate Vulkan buffer
	VkCommandBufferCreation,    ///< Failed to create command buffer(s)
	VkCommandPoolCreation,      ///< Failed to create command pool
	VkDebugMessengerCreation,   ///< Failed to create Vulkan debug messenger
	VkDescriptorCreation,       ///< Failed to create descriptor set
	VkDescriptorLayoutCreation, ///< Failed to create descriptor set layout
	VkDescriptorUpdate,         ///< Failed to update descriptor set
	VkDescriptorPoolCreation,   ///< Failed to create descriptor pool
	VkDeviceCreation,           ///< Failed to create Vulkan logical device
	VkFenceCreation,            ///< Failed to create synchronization fence
	VkInstanceCreation,         ///< Failed to create Vulkan instance
	VkPipelineCreation,         ///< Failed to create graphics/compute pipeline
	VkPipelineLayoutCreation,   ///< Failed to create pipeline layout
	VkQueueCreation,            ///< Failed to retrieve device queue
	VkSamplerCreation,          ///< Failed to create texture sampler
	VkSemaphoreCreation,        ///< Failed to create synchronization semaphore
	VkSurfaceCreation1,         ///< Primary surface creation failed
	VkSurfaceCreation2,         ///< Fallback surface creation failed
	VkSwapchainCreation,        ///< Failed to create swapchain
	VkSwapchainImagesCreation,  ///< Failed to retrieve swapchain images

	// Vulkan Memory Allocator (VMA) failures
	VMAInitialisation,          ///< Failed to initialize VMA
	VMAImageCreation,           ///< Failed to create VMA-backed image
	VMAImageViewCreation,       ///< Failed to create view for VMA image

	// Memory access failures
	MappedAccess,               ///< Failed to map memory for access

	// Shader related failures
	ComputeShader,              ///< Compute shader compilation/linking failed
	FragmentShader,             ///< Fragment shader compilation/linking failed
	VertexShader,               ///< Vertex shader compilation/linking failed

	First = ImguiContext,
	Last = VertexShader,
};


/**
 * @brief Exception class for graphics engine initialization failures
 *
 * Specialized exception that carries both a FailureType category and
 * detailed message about Vulkan/SDL/ImGui related failures during
 * engine startup and operation.
 */
class Failure
	: public std::exception
{
public:
	explicit Failure(FailureType type);
	Failure(FailureType type, const std::string &message);

	virtual const char* what() const noexcept override;

private:
	std::string msg;
};


}


#endif // GRAPHICS_ERROR_H

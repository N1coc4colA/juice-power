#ifndef ERROR_H
#define ERROR_H

#include <exception>
#include <string>


namespace Graphics {


enum FailureType
{
	ImguiContext,
	ImguiInitialisation,
	ImguiFontsInitialisation,
	ImguiVkInitialisation,
	SDLInitialisation,
	SDLWindowCreation,
	SDLVkSurfaceCreation,
	VkBufferAllocation,
	VkCommandBufferCreation,
	VkCommandPoolCreation,
	VkDebugMessengerCreation,
	VkDescriptorCreation,
	VkDescriptorLayoutCreation,
	VkDescriptorUpdate,
	VkDescriptorPoolCreation,
	VkDeviceCreation,
	VkFenceCreation,
	VkInstanceCreation,
	VkPipelineCreation,
	VkPipelineLayoutCreation,
	VkQueueCreation,
	VkSamplerCreation,
	VkSemaphoreCreation,
	VkSurfaceCreation1,
	VkSurfaceCreation2,
	VkSwapchainCreation,
	VkSwapchainImagesCreation,
	VMAInitialisation,
	VMAImageCreation,
	VMAImageViewCreation,

	MappedAccess,

	ComputeShader,
	FragmentShader,
	VertexShader,

	First = ImguiContext,
	Last = VertexShader,
};


class Failure
	: public std::exception
{
public:
	explicit Failure(FailureType type, const std::string &message);
	Failure(FailureType type);

	virtual const char* what() const noexcept override;

private:
	std::string msg;
};


}


#endif // ERROR_H

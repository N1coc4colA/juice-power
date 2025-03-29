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
	VkCommandBfferCreation,
	VkCommandPoolCreation,
	VkDescriptorCreation,
	VkDescriptorPoolCreation,
	VkDeviceCreation,
	VkFenceCreation,
	VkMessengerCreation,
	VkPipelineCreation,
	VkPipelineLayoutCreation,
	VkQueueCreation,
	VkSamplerCreation,
	VkSemaphoreCreation,
	VkSwapchainCreation,
	VMAInitialisation,
	VMAImageCreation,
	VMAImageViewCreation,

	PipelineCreation,

	First = PipelineCreation,
	Last = PipelineCreation,
};


class Exception
	: public std::exception
{
public:
	explicit Exception(FailureType type, const std::string &message);

	virtual const char* what() const noexcept override;

private:
	std::string msg;
};


}


#endif // ERROR_H

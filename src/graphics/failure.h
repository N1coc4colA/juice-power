#ifndef GRAPHICS_ERROR_H
#define GRAPHICS_ERROR_H

#include <exception>
#include <string>

#include "enums.h"

namespace Graphics
{

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

    virtual const char *what() const noexcept override;

private:
    std::string m_msg;
};


}


#endif // GRAPHICS_ERROR_H

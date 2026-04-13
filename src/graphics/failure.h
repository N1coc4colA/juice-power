#ifndef JP_GRAPHICS_ERROR_H
#define JP_GRAPHICS_ERROR_H

#include <exception>
#include <string>

#include "src/graphics/enums.h"
#include "src/keywords.h"

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
	/// @brief Builds a failure with the default message for the given type.
	explicit Failure(FailureType type);
	/// @brief Builds a failure with a custom detail message.
	Failure(FailureType type, const std::string &message);

    /// @brief Returns the formatted error message.
    _nodiscard auto what() const noexcept -> const char * override;

private:
    /**
     * @brief Failure message of the exception.
     */
    std::string m_msg{};
};


}

#endif // JP_GRAPHICS_ERROR_H

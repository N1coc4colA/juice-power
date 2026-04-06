#include "src/graphics/failure.h"

#include <magic_enum.hpp>


namespace Graphics
{

Failure::Failure(const FailureType type, const std::string &message)
	: m_msg(std::string(magic_enum::enum_name(type)) + message)
{
}

Failure::Failure(const FailureType type)
	: m_msg(magic_enum::enum_name(type))
{
}

auto Failure::what() const noexcept -> const char *
{
	return m_msg.c_str();
}


}

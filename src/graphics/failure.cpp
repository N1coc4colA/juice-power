#include "failure.h"

#include <magic_enum.hpp>


namespace Graphics
{

Failure::Failure(FailureType type, const std::string &message)
	: m_msg(std::string(magic_enum::enum_name(type)) + message)
{
}

Failure::Failure(FailureType type)
	: m_msg(magic_enum::enum_name(type))
{
}

const char *Failure::what() const noexcept
{
	return m_msg.c_str();
}


}

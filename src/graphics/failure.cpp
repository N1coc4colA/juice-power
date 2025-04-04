#include "failure.h"

#include <magic_enum.hpp>


namespace Graphics
{

Failure::Failure(FailureType type, const std::string &message)
	: msg(std::string(magic_enum::enum_name(type)) + message)
{
}

Failure::Failure(FailureType type)
	: msg(magic_enum::enum_name(type))
{
}

const char *Failure::what() const noexcept
{
	return msg.c_str();
}


}

#include "error.h"

#include <magic_enum.hpp>


namespace Graphics
{

Exception::Exception(FailureType type, const std::string &message)
	: msg(std::string(magic_enum::enum_name(type)) + message)
{
}

const char *Exception::what() const noexcept
{
	return msg.c_str();
}


}

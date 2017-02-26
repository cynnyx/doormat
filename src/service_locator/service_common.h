#pragma once

#include <stdexcept>

namespace service
{

//Exceptions
/**
 * @brief The not_initialized struct can be thrown to notify that
 * a resource has not been initialized yet.
 */
struct not_initialized : public std::runtime_error
{
	explicit not_initialized(const std::string& arg = "" ): std::runtime_error(arg) {}
};

/**
 * @brief The not_implemented struct can be thrown to notify that
 * a service is not implemented.
 */
struct not_implemented : public std::runtime_error
{
	explicit not_implemented(const std::string& arg = "" ): std::runtime_error(arg) {}
};

}//service


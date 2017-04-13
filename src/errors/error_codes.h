#ifndef HTTP_ERROR_CODES_H
#define HTTP_ERROR_CODES_H

#include <cstdint>
#include <ostream>
#include <string>
#include <cassert>
#include <iostream>
namespace errors
{

/**
 * @brief The http_error_code enum holds the http status
 * error codes available for doormat responses.
 */
enum class http_error_code : uint16_t
{
	bad_request = 400,
	forbidden = 403,
	not_found = 404,
	method_not_allowed = 405,
	request_timeout = 408,
	length_required = 411,
	entity_too_large = 413,
	uri_too_long = 414,
	unsupported_media_type = 415,
	unprocessable_entity = 422,
	internal_server_error = 500,
	bad_gateway = 502,
	service_unavailable = 503,
	gateway_timeout = 504,
	not_implemented = 501, // for unknown transfer encoding
};

/**
 * @brief to_string
 * @param e http error status
 * @return a lowercase string representing the name of the status
 */
inline
std::string to_string( http_error_code e )
{
	std::string ret;
	switch(e)
	{
	case http_error_code::bad_request:
		ret = "bad request";
		break;

	case http_error_code::forbidden:
		ret = "forbidden";
		break;
	case http_error_code::method_not_allowed:
		ret = "method not allowed";
		break;
	case http_error_code::request_timeout:
		ret = "request timeout";
		break;
	case http_error_code::internal_server_error:
		ret = "internal server error";
		break;
	case http_error_code::bad_gateway:
		ret = "bad gateway";
		break;
	case http_error_code::service_unavailable:
		ret = "service unavailable";
		break;
	case http_error_code::gateway_timeout:
		ret = "gateway timeout";
		break;
	case http_error_code::not_implemented:
		ret = "not implemented";
			break;
	case http_error_code::unprocessable_entity:
		ret = "unprocessable entity";
		break;
	case http_error_code::entity_too_large:
		ret = "entity too large";
		break;
    case http_error_code::not_found:
        ret = "not found";
        break;
    case http_error_code::length_required:
        ret = "length required";
        break;
    case http_error_code::uri_too_long:
        ret = "URI too long!";
        break;
    case http_error_code::unsupported_media_type:
        ret = "Unsupported media type.";
        break;
	default:
		ret = "Unknown error";
        break;
	}

	return ret;
}

/**
 * @brief operator << simply call the \ref to_string function
 * and put the string into the output stream.
 */
inline
std::ostream& operator<<( std::ostream& os, http_error_code e )
{
	return os << to_string(e);
}

}

#endif // HTTP_ERROR_CODES_H

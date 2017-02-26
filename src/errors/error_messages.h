#ifndef HTTP_ERROR_MESSAGES_H
#define HTTP_ERROR_MESSAGES_H

#include <functional>
#include <string>

#include "error_codes.h"
#include "cynnypp/async_fs.hpp"

/**
 * @brief The \ref errors namespace holds all the stuff relative
 * to the possible error responses sent by doormat.
 */
namespace errors
{

/**
 * @brief error_body takes care to prepare a response \ref body representing an html page
 * for an error status.
 *
 * @param file_path the path to the file containing the desired response body
 * @param on_msg_ready callback that will be called whenever the message will be ready
 */
void error_body(const std::string& file_path,
	std::function<void(const cynny::cynnypp::filesystem::ErrorCode& ec, std::string body)> on_msg_ready);

}

#endif // HTTP_ERROR_MESSAGES_H

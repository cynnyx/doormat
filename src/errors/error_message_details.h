#ifndef DOOR_MAT_ERROR_MESSAGE_DETAILS_H
#define DOOR_MAT_ERROR_MESSAGE_DETAILS_H

#include <string>
#include <functional>

#include "error_codes.h"
#include "../utils/resource_reader.h"

/**
 * @brief implementation details for the \ref errors namespace
 */
namespace detail
{

/**
 * @brief The error_message_factory class go to the disk and asynchronously
 * fetch an error html file; then, it returns its content in a callback.
 *
 * This class publically derives \ref utils::resource_reader but it
 * is not intended to be used polimorphically. Public inheritance is
 * due to the need of exposing std::enable_shared_from_this.
 */
class error_message_factory : public utils::resource_reader
{
public:
	using callback_t = std::function<void(const error_t& ec, std::string body)>;
	/**
	 * @brief error_message_factory performs all the work for the _interface function_ \ref error_body().
	 *
	 * @param file_path the path to the file containing the desired response body
	 * @param msg the message to be prepared
	 * @param cb function to be called when the message is ready
	 *
	 * @throw an error_t object if it's not possible to open the file;
	 * an std::bad_alloc if it's not possible to allocate memory for the body;
	 * an std::length_error if the space needed for the body is larger than
	 * the permitted max size.
	 */
	error_message_factory(const std::string& file_path, callback_t cb);

private:
	static void handle_chunk(error_message_factory* ptr, const error_t& ec, buffer_t chunk);
	static void handle_completion(error_message_factory* ptr, const error_t& ec, size_t bytes_read) noexcept;

	/**
	 * @brief do_body returns the body of an html page reporting an error.
	 *
	 * It guarantees that something meaningfull will be returned
	 * *only the first time* this function is called.
	 *
	 * @return body of the html page
	 */
	std::string do_body();

	static void replace_placeholder(std::string& str);

	callback_t cb;
	std::string data;
};

}



#endif //DOOR_MAT_ERROR_MESSAGE_DETAILS_H

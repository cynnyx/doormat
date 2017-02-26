#ifndef DOOR_MAT_ERROR_FILE_PROVIDER_H
#define DOOR_MAT_ERROR_FILE_PROVIDER_H

#include "../chain_of_responsibility/node_interface.h"
#include "../errors/error_factory_async.h"
#include "../log/access_record.h"

namespace nodes
{

class error_file_provider : public node_interface
{
	bool active{false};
	node_erased ne;
	dstring body;
	bool valid_method( http_method i_method  ) const noexcept;

	/**
	 * @brief path_to_file
	 * @param http_path
	 * @return an empty string if the path is forbidden (e.g. contains
	 * a ".."), the path to the file to be read otherwise.
	 */
	static dstring path_to_file(const dstring& http_path);
	static bool double_dot( const std::string& path );
public:
	error_file_provider(req_preamble_fn, req_chunk_fn,	req_trailer_fn, req_canceled_fn request_canceled, req_finished_fn,
			header_callback, body_callback, trailer_callback,end_of_message_callback,
			error_callback, response_continue_callback, logging::access_recorder *aclogger = nullptr);
	void on_request_preamble(http::http_request&& preamble);
	void on_request_body(dstring&& chunk);
	void on_request_trailer(dstring&& k, dstring&& v);
	void on_request_finished();
};

}

#endif //DOOR_MAT_ERROR_FILE_PROVIDER_H

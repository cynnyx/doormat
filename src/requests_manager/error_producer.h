#ifndef DOOR_MAT_ERROR_PRODUCER_H
#define DOOR_MAT_ERROR_PRODUCER_H

#include "../chain_of_responsibility/node_interface.h"
#include "../errors/error_factory_async.h"
#include "../log/access_record.h"

#include <string>

namespace nodes
{

/**
 * @brief The error_producer class intercepts on_error() events
 * so that the chain will send an error page as a response.
 *
 * Whenever a node that is after error_producer in the chain
 * triggers the on_error() event, every other event will be ignored
 * by discarded by the error_producer node, both it's upwards (response)
 * or downwards(request).
 */
class error_producer : public node_interface
{
	node_erased ne;
	std::shared_ptr<errors::error_factory_async> efa;
	std::string body;
	http::proto_version proto;
	errors::error_code code;

	// If there is an error code to be mapped, check this method!
	errors::http_error_code mapper () const noexcept;
public:
	error_producer(req_preamble_fn request_preamble,
		req_chunk_fn request_body,
		req_trailer_fn request_trailer,
		req_canceled_fn request_canceled,
		req_finished_fn request_finished,
		header_callback hcb,
		body_callback body,
		trailer_callback trailer,
		end_of_message_callback end_of_message,
		error_callback ecb,
		response_continue_callback rccb,logging::access_recorder *aclogger = nullptr);

	void on_error(const errors::error_code &ec);
	void on_header(http::http_response&&res);
	void on_request_preamble(http::http_request&& preamble);
	void on_request_body(dstring&& chunk);
	void on_request_trailer(dstring&& k, dstring&& v);
	void on_request_finished();

	bool client_stream_begun{false};
};

}
#endif //DOOR_MAT_ERROR_PRODUCER_H

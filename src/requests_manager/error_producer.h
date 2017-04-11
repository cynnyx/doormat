#ifndef DOOR_MAT_ERROR_PRODUCER_H
#define DOOR_MAT_ERROR_PRODUCER_H

#include "../chain_of_responsibility/node_interface.h"
#include "../errors/error_factory.h"
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
	std::string body;
    std::unique_ptr<errors::error_factory> efa{nullptr};
	http::proto_version proto = http::proto_version::HTTP11;
	errors::error_code code;
    bool management_enabled{true};
	errors::http_error_code mapper () const noexcept;

public:
	using node_interface::node_interface;

	void on_error(const errors::error_code &ec);

	void on_request_preamble(http::http_request&& preamble);
	void on_request_body(dstring&& chunk);
	void on_request_trailer(dstring&& k, dstring&& v);
	void on_request_finished();

    void on_header(http::http_response&&res);
    void on_body(dstring &&);
    void on_trailer(dstring &&, dstring &&);
    void on_end_of_message();
};

}
#endif //DOOR_MAT_ERROR_PRODUCER_H

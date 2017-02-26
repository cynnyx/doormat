#pragma once

#include "../chain_of_responsibility/node_interface.h"
#include "../chain_of_responsibility/node_erased.h"
#include "../errors/error_factory_async.h"

#include <memory>

namespace nodes
{

/**
 * @brief The header_filter class strips off transaction related headers
 * filters out request with unsupported transfer-encoding features
 * and temporarily bans requests containing "Expected: 100" on http2
 */
class header_filter : public node_interface
{
	bool rejected{false};
public:
	using node_interface::node_interface;
	void on_request_preamble(http::http_request&&);
	void on_request_body(dstring&&);
	void on_request_trailer(dstring&&,dstring&&);
	void on_request_finished();
};
} // namespace nodes


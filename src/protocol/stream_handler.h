#pragma once

#include <list>
#include <memory>

#include "handler_factory.h"
#include "../utils/log_wrapper.h"
#include "../chain_of_responsibility/node_interface.h"
#include "../chain_of_responsibility/chain_of_responsibility.h"

#include "../log/access_record.h"

namespace nghttp2{namespace asio_http2{ namespace server{
	struct response;
}}}

namespace server
{
using ng_res = nghttp2::asio_http2::server::response;
class handler_http2;

class stream_handler : public std::enable_shared_from_this<stream_handler>
{
	using request_t = http::http_request;
	using response_t = ng_res;

	bool _terminated{false};
	size_t partial_read_offset{0};
	handler_http2* _listener{nullptr};
	const response_t* _response{nullptr};

	std::list<dstring> buf_chain;
	std::unique_ptr<node_interface> managed_chain{nullptr};

	logging::access_recorder _access;

public:
	stream_handler(std::unique_ptr<node_interface> &&);
	~stream_handler(){ _access.commit(); }

	void listener(handler_http2 *l) noexcept { _listener = l; }
	void response(const response_t* p) noexcept { _response = p; }

	bool operator==(const stream_handler &other) { return managed_chain.get() == other.managed_chain.get(); }
	bool terminated() const noexcept {return _response == nullptr && _terminated; }

	void on_request_preamble(http::http_request&& message);
	void on_request_body(dstring&&);
	void on_request_finished();
	void on_header(http::http_response &&);
	void on_body(dstring&&);
	void on_trailer(dstring&&, dstring&&);
	void on_eom();
	void on_error(const int &);
	void on_response_continue();
	void on_request_canceled(const errors::error_code &ec);
};
}

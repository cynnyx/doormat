#ifndef DOOR_MAT_NODE_INTERFACE_H
#define DOOR_MAT_NODE_INTERFACE_H

#include <functional>
#include <utility>
#include <string>
#include <memory>

#include <assert.h>
#include <boost/noncopyable.hpp>

#include "../http/http_request.h"
#include "../http/http_response.h"
#include "error_code.h"
#include "../log/access_record.h"

/*Functions used to send data and events downstream*/
using req_preamble_fn = std::function<void(http::http_request &&)>;
using req_chunk_fn = std::function<void(std::unique_ptr<const char[]>, size_t)>;
using req_trailer_fn = std::function<void(std::string&&, std::string&&)>;
using req_finished_fn = std::function<void(void)>;
using req_canceled_fn = std::function<void(const errors::error_code &)>;

/** Functions used to send data and events upstream*/
using header_callback = std::function<void(http::http_response &&)>;
using body_callback = std::function<void(std::unique_ptr<const char[]>, size_t)>;
using trailer_callback = std::function<void(std::string&&, std::string&&)>;
using end_of_message_callback = std::function<void()>;
using error_callback = std::function<void(const errors::error_code &)>;
using response_continue_callback = std::function<void()>;

struct node_interface : private boost::noncopyable
{
	using base = node_interface;
	using data_t = std::unique_ptr<const char[]>;

	node_interface() = default;
	node_interface(req_preamble_fn request_preamble, req_chunk_fn request_body, req_trailer_fn request_trailer,
		req_canceled_fn request_canceled, req_finished_fn request_finished, header_callback hcb,
		body_callback body, trailer_callback trailer, end_of_message_callback end_of_message,
		error_callback ecb, response_continue_callback rccb,
		logging::access_recorder *access_recoder = nullptr)
		: request_preamble{request_preamble}
		, request_body{request_body}
		, request_trailer{request_trailer}
		, request_canceled{request_canceled}
		, request_finished{request_finished}
		, header{hcb}
		, body{body}
		, trailer{trailer}
		, end_of_message{end_of_message}
		, error{ecb}
		, response_continue{rccb}
		, aclogger{access_recoder}
	{}

	void initialize_callbacks(header_callback hcb, body_callback bcb, trailer_callback tcb, end_of_message_callback eomcb,
			error_callback ecb, response_continue_callback rccb)
	{
		header = hcb;
		body = bcb;
		trailer = tcb;
		end_of_message = eomcb;
		error = ecb;
		response_continue = rccb;
	}

	/** on_request preamble is used to propagate a request downstream; the ownership is mantained by the reader as long as
	 *  it is needed. Further, since it needs to be transformed/serialized before being transmitted, it is ok to use
	 *  the reference.
	 * */
	void on_request_preamble(http::http_request&& message) { request_preamble(std::move(message)); }
	void on_request_body(data_t data, size_t len) { request_body(std::move(data), len); }
	void on_request_trailer(std::string&& k, std::string&& v) { request_trailer(std::move(k),std::move(v)); }
	void on_request_finished(){ request_finished(); }
	void on_request_canceled(const errors::error_code&ec){ request_canceled(ec); }

	void on_header(http::http_response &&h) { header(std::move(h)); }
	void on_body(data_t data, size_t len) { body(std::move(data), len); }
	void on_trailer(std::string&& k, std::string&& v){trailer(std::move(k),std::move(v));}
	void on_end_of_message() { end_of_message(); }

	void on_error(const errors::error_code &ec) { error(ec); }

	//Handle HTTP1.1 CONTINUE
	void on_response_continue(){ response_continue(); }

	virtual ~node_interface() = default;

protected:
	logging::access_recorder & access_logger()
	{
		assert(aclogger);
		return *aclogger;
	}
private:
	//entry point not defined in derived class
	req_preamble_fn request_preamble = [](http::http_request&&) {assert(false);};
	req_chunk_fn request_body = [](auto&&, auto&&){assert(false); };
	req_trailer_fn request_trailer = [](std::string&&, std::string&&){assert(false); };
	req_canceled_fn request_canceled = [](const errors::error_code &){ assert(false); };
	req_finished_fn request_finished = [](){assert(false); };

	//triggers
	header_callback header = [](http::http_response &&){};
	body_callback body = [](auto&&, auto&&){};
	trailer_callback trailer = [](std::string&&,std::string&&){};
	end_of_message_callback end_of_message = [](){};
	error_callback error = [](const errors::error_code &){};

	//
	response_continue_callback response_continue = [](){};

	logging::access_recorder *aclogger{nullptr};
};


#endif //DOOR_MAT_NODE_INTERFACE_H

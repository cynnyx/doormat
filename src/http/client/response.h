#ifndef DOORMAT_CLIENT_RESPONSE_H
#define DOORMAT_CLIENT_RESPONSE_H

#include <memory>
#include <functional>
#include <experimental/optional>
#include <iostream>
#include "../http_response.h"
#include "../message_error.h"

namespace http2
{
class stream;
}


namespace server
{
template<typename handler_traits>
class handler_http1;
}

namespace http {
/** \brief the client_response class provides to the library user a mean through which it can subscribe to events related
 *  to the client_response.
 * */
class client_connection;

class client_response : public std::enable_shared_from_this<client_response> {
	template<typename> // TODO: only the client side handler should be friend
	friend class server::handler_http1;
	friend class http2::stream;
public:
	client_response(std::shared_ptr<client_connection>);
	//non-copiable object.
	client_response(const client_response&) = delete;
	client_response& operator=(const client_response&) = delete;

	/** Callback types. */
	using headers_callback_t = std::function<void(std::shared_ptr<client_response>)>;
	using body_callback_t = std::function<void(std::shared_ptr<client_response>, std::unique_ptr<char> char_array, size_t size)>;
	using trailer_callback_t = std::function<void(std::shared_ptr<client_response>, std::string k, std::string v)>;
	using finished_callback_t = std::function<void(std::shared_ptr<client_response>)>;
	using error_callback_t = std::function<void(std::shared_ptr<client_response>, const http::connection_error &err)>;

	void on_headers(headers_callback_t);
	void on_body(body_callback_t);
	void on_error(error_callback_t);
	void on_trailer(trailer_callback_t);
	void on_finished(finished_callback_t);

	/** Preamble manipulation methods */
	http::http_response & preamble() { return _preamble; }
	void clear_preamble() { _preamble = http::http_response{}; }

	/** Connection retrieve method*/
	std::shared_ptr<http::client_connection> get_connection();

private:

	/** External handlers allowing to trigger events to be communicated to the user */
	void headers(http::http_response &&);
	void body(dstring&& d);
	void error(http::connection_error err);
	void trailer(dstring&&, dstring&&);
	void finished();

	void init(){ myself = this->shared_from_this();}


	/* User registered events */
	std::experimental::optional<headers_callback_t> headers_callback;
	std::experimental::optional<body_callback_t> body_callback;
	std::experimental::optional<error_callback_t> error_callback;
	std::experimental::optional<trailer_callback_t> trailer_callback;
	std::experimental::optional<finished_callback_t> finished_callback;

	std::shared_ptr<client_connection> connection_keepalive;
	/** Ptr-to-self: to grant the user that, until finished() or error() event is propagated, the client_response will be alive*/
	std::shared_ptr<client_response> myself;

	http::connection_error conn_error{http::error_code::success};

	http::http_response _preamble;

};


}


#endif // DOORMAT_CLIENT_RESPONSE_H

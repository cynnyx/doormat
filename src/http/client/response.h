#ifndef DOORMAT_CLIENT_RESPONSE_H
#define DOORMAT_CLIENT_RESPONSE_H

#include <memory>
#include <functional>
#include <boost/asio/io_service.hpp>
#include <iostream>



#include "../http_response.h"
#include "../message_error.h"

namespace http2
{
class stream_client;
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
struct client_traits;

class client_response : public std::enable_shared_from_this<client_response> {
	friend class server::handler_http1<client_traits>;
	friend class http2::stream_client;
public:
	client_response(std::shared_ptr<client_connection>, boost::asio::io_service &io);
	//non-copiable object.
	client_response(const client_response&) = delete;
	client_response& operator=(const client_response&) = delete;

	/** Callback types. */
	using data_t = std::unique_ptr<const char[]>;
	using headers_callback_t = std::function<void(std::shared_ptr<client_response>)>;
	using body_callback_t = std::function<void(std::shared_ptr<client_response>, data_t char_array, size_t size)>;
	using trailer_callback_t = std::function<void(std::shared_ptr<client_response>, std::string k, std::string v)>;
	using finished_callback_t = std::function<void(std::shared_ptr<client_response>)>;
	using error_callback_t = std::function<void(std::shared_ptr<client_response>, const http::connection_error &err)>;
	using continue_callback_t = std::function<void(std::shared_ptr<client_response>)>;

	void on_headers(headers_callback_t);
	void on_body(body_callback_t);
	void on_error(error_callback_t);
	void on_trailer(trailer_callback_t);
	void on_finished(finished_callback_t);
	void on_response_continue(continue_callback_t);

	/** Preamble manipulation methods */
	http::http_response & preamble() { return _preamble; }
	void clear_preamble() { _preamble = http::http_response{}; }

	/** Connection retrieve method*/
	std::shared_ptr<http::client_connection> get_connection();

private:

	/** External handlers allowing to trigger events to be communicated to the user */
	void headers(http::http_response &&);
	void body(data_t, size_t);
	void error(http::connection_error err);
	void trailer(std::string&&, std::string&&);
	void finished();
	void response_continue();

	bool ended() const noexcept { return response_ended; }

	/* User registered events */
	headers_callback_t headers_callback;
	body_callback_t body_callback;
	error_callback_t error_callback;
	trailer_callback_t trailer_callback;
	finished_callback_t finished_callback;
	continue_callback_t continue_callback;

	std::shared_ptr<client_connection> connection_keepalive;
	http::connection_error conn_error{http::error_code::success};

	http::http_response _preamble;

	bool response_ended{false};

	boost::asio::io_service &io;
};


}


#endif // DOORMAT_CLIENT_RESPONSE_H

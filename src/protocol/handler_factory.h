#pragma once

#include <memory>
#include <functional>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include "../../deps/openssl/include/openssl/ssl.h"
#include "../http/http_commons.h"
#include "../http/connection.h"
#include "http_handler.h"
#include "../http2/session.h"
#include "handler_http1.h"
#include "../http/server/server_traits.h"

#include "../connector.h"



class dstring;
struct node_interface;

namespace http
{
class server_connection;
class http_response;
}

namespace server
{

class http_handler;

using interval = boost::posix_time::time_duration;
using tcp_socket = boost::asio::ip::tcp::socket;
using ssl_socket = boost::asio::ssl::stream<tcp_socket>;

enum class handler_type
{
	ht_h1,
	ht_h2
};

class handler_factory
{


public:
	void register_protocol_selection_callbacks(SSL_CTX* ctx);
	std::shared_ptr<http::server_connection> negotiate_handler(std::shared_ptr<ssl_socket> s) const noexcept;

	// specializations
	template<typename T>
	std::shared_ptr<http::server_connection> build_handler(handler_type type, http::proto_version proto, std::shared_ptr<T> socket) const noexcept
	{
		auto conn = std::make_shared<connector<T>>(socket);
		if(type == handler_type::ht_h2)
		{
			auto h = std::make_shared<http2::session>();
			conn->handler(h);
			conn->start(true);
			return h;
		} else {
			auto h = std::make_shared<handler_http1<http::server_traits>>();
			conn->handler(h);
			conn->start(true);
			return h;
		}
	}
};

} //namespace

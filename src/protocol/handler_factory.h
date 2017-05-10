#pragma once

#include <memory>
#include <functional>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include "../../deps/openssl/include/openssl/ssl.h"
#include <iostream>
#include <experimental/optional>
#include "../http/http_commons.h"
#include "../http/connection.h"
#include "handler_interface.h"

class dstring;
struct node_interface;

namespace http
{
class http_response;
}

namespace server
{
using interval = boost::posix_time::time_duration;


enum class handler_type
{
	ht_h1,
	ht_h2
};

class connector_interface;
using tcp_socket = boost::asio::ip::tcp::socket;
using ssl_socket = boost::asio::ssl::stream<tcp_socket>;


class handler_factory
{
	std::shared_ptr<handler_interface> make_handler(handler_type, http::proto_version) const noexcept;

public:
	void register_protocol_selection_callbacks(SSL_CTX* ctx);
	std::shared_ptr<handler_interface> negotiate_handler(std::shared_ptr<ssl_socket> s,interval, interval) const noexcept;
	std::shared_ptr<handler_interface> build_handler(handler_type, http::proto_version vers, interval, interval, std::shared_ptr<ssl_socket> s) const noexcept;
	std::shared_ptr<handler_interface> build_handler(handler_type, http::proto_version vers, interval, interval, std::shared_ptr<tcp_socket> socket) const noexcept;
};

} //namespace

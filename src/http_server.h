#pragma once

#include <string>
#include <memory>
#include <atomic>

#include <experimental/optional>
#include <boost/asio.hpp>
#include <boost/system/error_code.hpp>
#include <fstream>

#include "utils/sni_solver.h"
#include "protocol/handler_factory.h"

namespace http {
class server_connection;
}

namespace server
{

using ssl_context = boost::asio::ssl::context;
using tcp_acceptor = boost::asio::ip::tcp::acceptor;

/** \brief http_server class allows to spawn an http server listenign on a tls and on a http port.
 *  Default ports are 443 and 80.
 **/
class http_server
{
	using connect_callback = std::function<void(std::shared_ptr<http::server_connection>)>;
	std::atomic_bool running{false};
	handler_factory _handlers;
	interval _connect_timeout;
	ssl_context* _ssl_ctx = nullptr; // a non-owner pointer to the ssl_context
	ssl_utils::sni_solver sni;
	bool _ssl;
	uint16_t ssl_port;
	uint16_t http_port;
	std::experimental::optional<tcp_acceptor> plain_acceptor;
	std::experimental::optional<tcp_acceptor> ssl_acceptor;
	void start_accept(tcp_acceptor&);
	void start_accept(ssl_context& , tcp_acceptor& );
	static tcp_acceptor make_acceptor(boost::asio::io_service &io, boost::asio::ip::tcp::endpoint endpoint, boost::system::error_code&);
	void listen(boost::asio::io_service &io, bool ssl = false );
	std::experimental::optional<connect_callback> connect_cb;
public:
	// If ssl_port is 0 tls is disabled
	//todo remove read_timeout and connect_timeout
	explicit http_server(size_t connect_timeout, uint16_t ssl_port, uint16_t http_port);

	http_server(const http_server&) = delete;
	http_server& operator=(const http_server&) = delete;

	void add_certificate(const std::string &cert, const std::string &key, const std::string &pass);

	void on_client_connect(connect_callback cb) noexcept;

	void start(boost::asio::io_service &io) noexcept;
	void stop() noexcept;
};

} // namespace server

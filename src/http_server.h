#pragma once

#include <string>
#include <vector>
#include <memory>
#include <atomic>

#include <experimental/optional>
#include <boost/asio.hpp>
#include <boost/noncopyable.hpp>
#include <boost/system/error_code.hpp>

#include "connector.h"
#include "io_service_pool.h"
#include "utils/sni_solver.h"
#include "protocol/handler_factory.h"

namespace server
{

using ssl_context = boost::asio::ssl::context;
using tcp_acceptor = boost::asio::ip::tcp::acceptor;
using tcp_connector = connector<tcp_socket>;
using ssl_connector = connector<ssl_socket>;

class http_server : private boost::noncopyable
{
	std::atomic_bool running{false};

    size_t _backlog;
	handler_factory _handlers;

	interval _read_timeout;
	interval _connect_timeout;

	ssl_context* _ssl_ctx = nullptr; // a non-owner pointer
	ssl_utils::sni_solver sni;
	bool _ssl;

	uint16_t ssl_port;
	uint16_t http_port;


	std::experimental::optional<tcp_acceptor> plain_acceptor;
	std::experimental::optional<tcp_acceptor> ssl_acceptor;



	void start_accept(tcp_acceptor&);
	void start_accept(ssl_context& , tcp_acceptor& );

	tcp_acceptor make_acceptor(boost::asio::ip::tcp::endpoint endpoint, boost::system::error_code&);
	void listen_on( const uint16_t &port, bool ssl = false );

public:
	explicit http_server(size_t read_timeout, size_t connect_timeout, uint16_t ssl_port = 443, uint16_t http_port = 80);
    void start(io_service_pool::main_init_fn_t main_init,
                io_service_pool::thread_init_fn_t thread_init = {}) noexcept;
	void stop() noexcept;

};

} // namespace server

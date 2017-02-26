#pragma once

#include <string>
#include <vector>
#include <memory>
#include <atomic>

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
	/**
	 * the SO_REUSEPORT is not available in boost::asio
	 * so we define it by ourselves.
	 */
	class reuse_port;
	std::atomic_bool running{false};
	size_t _backlog;
	size_t _threads;

	handler_factory _handlers;

	interval _read_timeout;
	interval _connect_timeout;

	ssl_context* _ssl_ctx = nullptr; // a non-owner pointer
	ssl_utils::sni_solver sni;

	std::vector<tcp_acceptor> _acceptors;
	std::vector<tcp_acceptor> _ssl_acceptors;

	void start_accept(tcp_acceptor&);
	void start_accept(ssl_context& , tcp_acceptor& );

	tcp_acceptor make_acceptor(boost::asio::ip::tcp::endpoint endpoint, boost::system::error_code&);
	boost::system::error_code listen_on( const uint16_t &port, bool ssl = false );

public:
	explicit http_server();
	void start(io_service_pool::main_init_fn_t& main_init) noexcept;
	void stop() noexcept;

};

} // namespace server

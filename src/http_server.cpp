#include <boost/lexical_cast.hpp>
#include "http_server.h"
#include "service_locator/service_initializer.h"
#include "utils/log_wrapper.h"
#include "network/cloudia_pool.h"
#include "network/communicator/dns_communicator_factory.h"

using namespace std;
using namespace boost::asio;
using namespace boost::asio::ip;

namespace server
{

void log_ssl_errors(const boost::system::error_code& ec)
{
	string err{" ("};
	err += boost::lexical_cast<string>(ERR_GET_LIB(ec.value()));
	err +=",";
	err += boost::lexical_cast<string>(ERR_GET_FUNC(ec.value()));
	err +=",";
	err += boost::lexical_cast<string>(ERR_GET_REASON(ec.value()));
	err +=") ";

	char buf[128];
	ERR_error_string_n(ec.value(), buf, sizeof(buf));
	err += buf;
	LOGERROR(err);
}

http_server::http_server()
	: _backlog( socket_base::max_connections )
	, _threads(service::locator::configuration().get_thread_number())
	, _read_timeout(boost::posix_time::milliseconds( service::locator::configuration().get_operation_timeout() ) )
	, _connect_timeout( boost::posix_time::milliseconds(
				service::locator::configuration().get_client_connection_timeout() ) )
	, _ssl{sni.load_certificates()}
{
	if(_ssl)
	{
		_ssl_ctx = &(sni.begin()->context);

		for( auto&& iter = sni.begin(); iter != sni.end(); ++iter )
			_handlers.register_protocol_selection_callbacks(iter->context.native_handle());

		auto port = service::locator::configuration().get_port();
		listen_on(port, true );
	}

	auto porth = service::locator::configuration().get_port_h();
	listen_on(porth);
}

void http_server::start(io_service_pool::main_init_fn_t main_init,
                        io_service_pool::thread_init_fn_t thread_init) noexcept
{
	if(!running)
	{
		running = true;

		for (auto &acceptor : _acceptors)
			start_accept(acceptor);

		for (auto &acceptor : _ssl_acceptors)
		{
			assert(_ssl_ctx != nullptr);
			start_accept(*_ssl_ctx , acceptor);
		}

        auto thread_init_local = [ti=std::move(thread_init)](boost::asio::io_service& ios)
		{
			using namespace service;
			locator::stats_manager().register_handler();
// 			initializer::set_socket_pool(new network::magnet(1));

			auto&& cw = locator::configuration();
			auto il = new logging::inspector_log{ cw.get_log_path(), "inspector", cw.inspector_active() };
			initializer::set_inspector_log(il);
			initializer::set_communicator_factory(new network::dns_communicator_factory());
            if(ti)
                ti(ios);
		};

		LOGINFO("Starting doormat on ports ", service::locator::configuration().get_port(),",",
				service::locator::configuration().get_port_h(),", with ", _threads, " threads");

        service::locator::service_pool().run(std::move(thread_init_local), std::move(main_init));
	}
}

void http_server::stop( ) noexcept
{
	if(running)
	{
		running = false;

		for (auto &acceptor : _acceptors)
			acceptor.close();

		for (auto &acceptor : _ssl_acceptors)
			acceptor.close();
	}
}

void http_server::start_accept(ssl_context& ssl_ctx, tcp_acceptor& acceptor)
{
	if(running.load() == false)
		return;

	auto socket = std::make_shared<ssl_socket>(acceptor.get_io_service(), ssl_ctx);
	acceptor.async_accept(socket->lowest_layer(),[this, &ssl_ctx, &acceptor, socket]( const boost::system::error_code &ec)
	{
		LOGTRACE("secure_accept_cb called");

		if(ec == boost::system::errc::operation_canceled)
			return;

		if (!ec)
		{
			auto conn = std::make_shared<ssl_connector>(_connect_timeout, _read_timeout, socket);
			auto handshake_cb = [this, conn](const boost::system::error_code &ec)
			{
				LOGTRACE("handshake_cb called");
				if (!ec)
				{
					auto h = _handlers.negotiate_handler(conn->socket().native_handle());
					if(h != nullptr)
					{
						conn->handler( h );
						conn->start(true);
						return;
					}
				}

				LOGERROR(this," async accept failed:", ec.message());
				if(ec.category() == boost::asio::error::get_ssl_category())
					log_ssl_errors(ec);
			};

			conn->handshake_countdown();
			conn->socket().async_handshake(ssl::stream_base::server, handshake_cb);
		}
		else LOGERROR(ec.message());

		start_accept(ssl_ctx, acceptor);
	});

}

void http_server::start_accept(tcp_acceptor& acceptor)
{
	if(running.load() == false)
		return;

	auto socket = std::make_shared<tcp_socket>(acceptor.get_io_service());
	acceptor.async_accept(socket->lowest_layer(),[this, &acceptor, socket](const boost::system::error_code& ec)
	{
		LOGTRACE("accept_cb called");
		if(ec == boost::system::errc::operation_canceled)
			return;

		if (!ec)
		{
			auto conn = std::make_shared<tcp_connector>(_connect_timeout, _read_timeout, socket);
			//Assume only HTTP1.1 can land here
			auto h = _handlers.build_handler(ht_h1);
			conn->handler( h );
			conn->start();
			return start_accept(acceptor);
		}
		else LOGERROR(ec.message());

		start_accept(acceptor);
	});
}

tcp_acceptor http_server::make_acceptor(tcp::endpoint endpoint, boost::system::error_code& ec)
{
	auto acceptor = tcp::acceptor(service::locator::service_pool().get_io_service());
	int set = 1;
	acceptor.open(endpoint.protocol(), ec);
	if(setsockopt(acceptor.native_handle(), SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &set, sizeof(set)) != 0)
	{
		LOGERROR("cannot set option SO_REUSEPORT on the socket; will execute in a sequential manner. Error is ", strerror(errno));
	}
	if(!ec)
		acceptor.bind(endpoint, ec);
	if(!ec)
		acceptor.listen(_backlog, ec);

	return acceptor;
}

void http_server::listen_on(const uint16_t &port, bool ssl )
{
	tcp::resolver resolver(service::locator::service_pool().get_io_service());
	tcp::resolver::query query("0.0.0.0", to_string(port));

	auto& acceptors = ssl?_ssl_acceptors:_acceptors;

	boost::system::error_code ec;
	auto it = resolver.resolve(query, ec);
	if (!ec)
	{
		for (; it != tcp::resolver::iterator(); ++it)
		{
			for(size_t i = 0; i < _threads; ++i)
			{
				auto acceptor = make_acceptor(*it, ec);
				if(!ec)
					acceptors.push_back(std::move(acceptor));
			}
		}
	}

	if(!acceptors.empty())
		ec.clear();


	if(ec)
	{
		LOGERROR("Error while listening on ", to_string(port));
		throw ec;
	}
}

}//namespace

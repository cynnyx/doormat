#include "http_server.h"
#include "utils/log_wrapper.h"
#include "http/server/server_connection.h"
#include "http/client/client_connection.h"
#include "service_locator/service_locator.h"
#include <boost/lexical_cast.hpp>
#include "./configuration/configuration_wrapper.h"

using namespace std;
using namespace boost::asio;
using namespace boost::asio::ip;

namespace server
{

namespace details {
    void log_ssl_errors(const boost::system::error_code &ec) {
        string err{" ("};
        err += boost::lexical_cast<string>(ERR_GET_LIB(ec.value()));
        err += ",";
        err += boost::lexical_cast<string>(ERR_GET_FUNC(ec.value()));
        err += ",";
        err += boost::lexical_cast<string>(ERR_GET_REASON(ec.value()));
        err += ") ";

        char buf[128];
        ERR_error_string_n(ec.value(), buf, sizeof(buf));
        err += buf;
        LOGERROR(err);
    }
}

http_server::http_server(size_t connect_timeout, uint16_t ssl_port, uint16_t http_port)
	:
	 _connect_timeout( boost::posix_time::milliseconds(connect_timeout)),
	 _ssl{ssl_port != 0},
     ssl_port{ssl_port},
     http_port{http_port}
{}

void http_server::on_client_connect(connect_callback cb) noexcept
{
    connect_cb.emplace(std::move(cb));
}

void http_server::start(boost::asio::io_service &io) noexcept
{
    if(running) return;
    running = true;
	_ssl = _ssl && sni.load_certificates();
	if(_ssl)
	{
		_ssl_ctx = &(sni.begin()->context);
		for(auto&& iter = sni.begin(); iter != sni.end(); ++iter)
			_handlers.register_protocol_selection_callbacks(iter->context.native_handle());
		listen(io, true);
    }

    listen(io);

	if(plain_acceptor) start_accept(*plain_acceptor);

    if(ssl_acceptor)
	{
		assert(_ssl_ctx != nullptr);
		start_accept(*_ssl_ctx, *ssl_acceptor);
	}

	//LOGINFO("Starting doormat on ports ", http_port ,",", ssl_port,", with ", 1, " threads");
}

void http_server::stop( ) noexcept
{
	if(running)
	{
		running = false;
		boost::system::error_code ec;
		if(plain_acceptor) plain_acceptor->close(ec);
		if(ssl_acceptor) ssl_acceptor->close(ec);
	}
}

void http_server::start_accept(ssl_context& ssl_ctx, tcp_acceptor& acceptor)
{
	if(running.load() == false)
		return;
	auto socket = std::make_shared<ssl_socket>(acceptor.get_io_service(), ssl_ctx);
	acceptor.async_accept(socket->lowest_layer(),[this, &ssl_ctx, &acceptor, socket]( const boost::system::error_code &ec)
	{
		//LOGTRACE("secure_accept_cb called");

		if(ec == boost::system::errc::operation_canceled)
			return;

		if (!ec)
		{
            auto connection_timer = std::make_shared<boost::asio::deadline_timer>(acceptor.get_io_service());
            connection_timer->expires_from_now(_connect_timeout);
            connection_timer->async_wait([socket, connection_timer](const boost::system::error_code &ec)
            {
                if(ec) return;
	            boost::system::error_code shutdown_error;
	            socket->shutdown(shutdown_error);
            });
			auto handshake_cb = [this, connection_timer, socket](const boost::system::error_code &ec)
			{
				//LOGTRACE("handshake_cb called");
				if(ec != boost::system::errc::operation_canceled)
                {
                    connection_timer->cancel();
                }
                if (!ec)
				{
					auto h = _handlers.negotiate_handler(socket);
                    // the check on h != nullptr is needed, because the protocol negotiation could fail.
                    // in the case without tls, instead, it is not needed as an handler (http1.x) will
                    // always be provided.
					if(h != nullptr && connect_cb)
					{
						return (*connect_cb)(h);
					}
				}
				//LOGERROR(this," async accept failed:", ec.message());
				if(ec.category() == boost::asio::error::get_ssl_category())
					details::log_ssl_errors(ec);
			};
            socket->async_handshake(ssl::stream_base::server, handshake_cb);
		}
	//	else //LOGERROR(ec.message());

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
		//LOGTRACE("accept_cb called");
		if(ec == boost::system::errc::operation_canceled)
			return;

		if (!ec)
		{
			//auto conn = std::make_shared<tcp_connector>(_connect_timeout, _read_timeout, socket);
			auto h = _handlers.build_handler(handler_type::ht_h1, http::proto_version::UNSET, socket);
            if(connect_cb)
            {
                (*connect_cb)(h);
            }
			return start_accept(acceptor);
		}
		else //LOGERROR(ec.message());

		start_accept(acceptor);
	});
}

tcp_acceptor http_server::make_acceptor(boost::asio::io_service& io, tcp::endpoint endpoint, boost::system::error_code& ec)
{
	auto acceptor = tcp::acceptor(io);
	int set = 1;
	acceptor.open(endpoint.protocol(), ec);
	if(setsockopt(acceptor.native_handle(), SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &set, sizeof(set)) != 0)
	{
		//LOGERROR("cannot set option SO_REUSEPORT on the socket; doormat will execute in a sequential manner. Error is ", strerror(errno));
	}
	if(!ec)
		acceptor.bind(endpoint, ec);
	if(!ec)
		acceptor.listen(socket_base::max_connections, ec);

	return acceptor;
}

void http_server::listen(boost::asio::io_service &io, bool ssl )
{
	auto port = (ssl) ? ssl_port : http_port;
	auto& acceptor = (ssl) ? ssl_acceptor : plain_acceptor;
	tcp::resolver resolver(io);
	//todo: make the interface addr. parametric in the constructor.
	tcp::resolver::query query("0.0.0.0", to_string(port));

	boost::system::error_code ec;
	auto it = resolver.resolve(query, ec);
	if (!ec)
	{
		for (; it != tcp::resolver::iterator(); ++it)
		{
            auto _acceptor = make_acceptor(io, *it, ec);
            if(!ec)
                acceptor = std::move(_acceptor);
		}
	}

	if(!acceptor)
		ec.clear();

	if(ec)
	{
		//LOGERROR("Error while listening on ", to_string(port));
		throw ec;
	}
}

void http_server::add_certificate(const std::string &cert, const std::string &key, const std::string &pass)
{
	if(running.load()) throw std::invalid_argument{"Could not add certificate when the server is running"};
	sni.add_certificate(cert, key, pass);
}

}//namespace

#include "dns_communicator_factory.h"
#include "../../io_service_pool.h"
#include "../../connector.h"
#include "../../service_locator/service_locator.h"
#include "../../configuration/configuration_wrapper.h"

#include <chrono>

namespace network 
{

void dns_connector_factory::get_connector(const std::string& address, uint16_t port, bool tls,
	connector_callback_t connector_cb, error_callback_t error_cb)
{
	if ( stopping ) return error_cb(1);

	dns_resolver(address, port, tls, std::move(connector_cb), std::move(error_cb));
}

void dns_connector_factory::dns_resolver(const std::string& address, uint16_t port, bool tls,
	connector_callback_t connector_cb, error_callback_t error_cb)
{
	auto&& io = service::locator::service_pool().get_thread_io_service();
	std::shared_ptr<boost::asio::ip::tcp::resolver> r = std::make_shared<boost::asio::ip::tcp::resolver>(io);
	if(!port)
		port = tls ? 443 : 80;

	LOGTRACE("resolving ", address, "with port ", port);
	boost::asio::ip::tcp::resolver::query q( address,  std::to_string(port) );
	auto resolve_timer =
		std::make_shared<boost::asio::deadline_timer>(service::locator::service_pool().get_thread_io_service());
	resolve_timer->expires_from_now(boost::posix_time::milliseconds(resolve_timeout));
	resolve_timer->async_wait([r, resolve_timer](const boost::system::error_code &ec)
	{
		if ( ! ec ) r->cancel();
	});

	r->async_resolve(q,
		[this, r, tls, connector_cb = std::move(connector_cb), error_cb = std::move(error_cb), resolve_timer]
		(const boost::system::error_code &ec, boost::asio::ip::tcp::resolver::iterator iterator)
		{
			resolve_timer->cancel();
			if(ec || iterator == boost::asio::ip::tcp::resolver::iterator())
			{
				LOGERROR(ec.message());
				return error_cb(3);
			}
			/** No error: go on connecting */
			LOGTRACE( "tls is: ", tls );
			if ( tls )
			{
				// FIXME we need to initialize it somewhere else! Locator?
				static thread_local boost::asio::ssl::context ctx(boost::asio::ssl::context::sslv23);
				return endpoint_connect(std::move(iterator),
					std::make_shared<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>>
						(service::locator::service_pool().get_thread_io_service(), ctx ),
							std::move(connector_cb), std::move(error_cb));
			}
			else
				return endpoint_connect(std::move(iterator),
					std::make_shared<boost::asio::ip::tcp::socket>
						(service::locator::service_pool().get_thread_io_service()),
							std::move(connector_cb), std::move(error_cb));
		});
}

void dns_connector_factory::endpoint_connect(boost::asio::ip::tcp::resolver::iterator it, 
	std::shared_ptr<boost::asio::ip::tcp::socket> socket, connector_callback_t connector_cb, error_callback_t error_cb)
{
	if(it == boost::asio::ip::tcp::resolver::iterator() || stopping) //finished
		return error_cb(3);
	auto&& endpoint = *it;
	auto connect_timer = 
		std::make_shared<boost::asio::deadline_timer>(service::locator::service_pool().get_thread_io_service());
	connect_timer->expires_from_now(boost::posix_time::milliseconds(connect_timeout));
	connect_timer->async_wait([socket, connect_timer](const boost::system::error_code &ec)
	{
		if(!ec) socket->cancel();
	});
	++it;
	socket->async_connect(endpoint,
		[this, it = std::move(it), socket, connector_cb = std::move(connector_cb), 
			error_cb = std::move(error_cb), connect_timer]
		(const boost::system::error_code &ec)
		{
			connect_timer->cancel();
			if ( ec )
			{
				LOGERROR(ec.message());
				return endpoint_connect(std::move(it), std::move(socket), std::move(connector_cb), std::move(error_cb));
			}

			auto connector = std::make_shared<server::connector<server::tcp_socket>>(std::move(socket));
			auto btimeout = service::locator::configuration().get_board_timeout();
			connector->set_timeout(std::chrono::milliseconds{btimeout});
			connector_cb(std::move(connector));
		});
}

void dns_connector_factory::endpoint_connect(boost::asio::ip::tcp::resolver::iterator it,
	std::shared_ptr<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>> stream, 
	connector_callback_t connector_cb, error_callback_t error_cb)
{
	if ( it == boost::asio::ip::tcp::resolver::iterator() || stopping ) //finished
		return error_cb(3);
	
	stream->set_verify_mode( boost::asio::ssl::verify_none );

	auto connect_timer = 
		std::make_shared<boost::asio::deadline_timer>(service::locator::service_pool().get_thread_io_service());
	connect_timer->expires_from_now(boost::posix_time::milliseconds(connect_timeout));
	connect_timer->async_wait([stream, connect_timer](const boost::system::error_code &ec)
	{
		if ( !ec ) //stream->cancel();
		{
			boost::system::error_code sec;
			stream->shutdown( sec );
		}
	});
	
	boost::asio::async_connect( stream->lowest_layer(), it, [ this, stream, connector_cb = std::move(connector_cb), 
		error_cb = std::move(error_cb), connect_timer=std::move(connect_timer) ]( const boost::system::error_code &ec,
			boost::asio::ip::tcp::resolver::iterator it )
	{
		if ( ec ) 
		{
			LOGERROR(ec.message());
			//++it ?
			return endpoint_connect(std::move(it), std::move(stream), std::move(connector_cb), std::move(error_cb));
		}
		stream->async_handshake( boost::asio::ssl::stream_base::client, 
			[ this, stream, connector_cb = std::move(connector_cb), error_cb = std::move(error_cb), connect_timer=std::move(connect_timer) ]
				( const boost::system::error_code &ec )
				{
					connect_timer->cancel();
					if ( ec )
					{
						LOGERROR( ec.message() );
						return;
					}
					int64_t btimeout = static_cast<int64_t>( service::locator::configuration().get_board_timeout() );
					auto connector = std::make_shared<server::connector<server::ssl_socket>>(std::move(stream));
					connector->set_timeout(std::chrono::milliseconds{btimeout});
					connector_cb(std::move(connector));
				});
	});
}

}

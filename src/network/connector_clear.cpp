#include "connector_clear.h"
#include "communicator.h"
#include "../service_locator/service_locator.h"
#include "cloudia_pool.h"
#include "buffer.h"

#include <boost/asio.hpp>

namespace network
{

connector_clear::connector_clear( const http::http_request &req, receiver& recv ): connector{req, recv}
{
	auto self = shared_from_this();
	auto&& socket_factory = service::locator::socket_pool_factory<socket>().get_socket_factory(0);
	socket_factory->get_socket( req, [self]
		( std::unique_ptr<network::socket_factory<connector_clear::socket>::socket_type>&& st )
		{
			if ( st )
			{
				self->output.set_communicator( 
					std::unique_ptr<network::communicator<connector_clear::socket>>
						{ 
							new communicator<>(std::move(st), [self](const char *data, size_t size)
							{
								if ( ! self->codec.decode( data, size ) )
								{
									self->on_error( INTERNAL_ERROR_LONG( errors::http_error_code::internal_server_error ) );
									self->stop();
								}
							}, [self] ( errors::error_code errc ) 
							{ 
								self->on_error( errc );
								self->stop();
							} ) 
						} );
			}
			else 
				self->on_error(667);
		}, [self]() mutable
		{
			self->on_error(666);
			self->stop();
		} );
	
	close_on_eom = ! req.keepalive();
	
	// Select Http1 Encoder decoder
	dstring msg = codec.encode_header( req );
	output.write( std::move( msg ) );
}

void connector_clear::send_body( dstring&& body ) noexcept
{
	dstring enc_body = codec.encode_body( body );
	output.write( std::move( enc_body ) );
}

void connector_clear::send_trailer( dstring&& key, dstring&& val ) noexcept
{
	dstring enc_trailer = codec.encode_trailer( key, val );
	output.write( std::move( enc_trailer ) );
}

void connector_clear::send_eom() noexcept 
{
	// Really useful? HTTP 1 or 1.1?
}

void connector_clear::on_header(http::http_response && r ) noexcept 
{
	rec.on_header( std::move( r ) );
}
void connector_clear::on_body(dstring&& body) noexcept
{
	rec.on_body( std::move( body ) );
}

void connector_clear::on_trailer(dstring&& key, dstring&& val) noexcept
{
	rec.on_trailer( std::move(key), std::move(val) );
}

void connector_clear::on_eom() noexcept 
{
	rec.on_eom();
	if ( close_on_eom )
		stop();
}

void connector_clear::stop() noexcept
{
	// buffer will be stopped on destruction
	rec.stop();
}
void connector_clear::on_error( int error ) noexcept 
{
	rec.on_error( error );
}

}

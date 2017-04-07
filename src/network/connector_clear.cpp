#include "connector_clear.h"
#include "communicator.h"
#include "../service_locator/service_locator.h"
#include "cloudia_pool.h"
#include "buffer.h"

#include <boost/asio.hpp>

namespace network
{
void connector_clear::on_response_continue() noexcept
{
	//@todo
}
	
void connector_clear::init() noexcept
{
	auto self = shared_from_this();
	
	codec.register_callback( [self](http::http_structured_data** d)
		{
			LOGTRACE("started");
			if ( self->http_continue ) { self->http_continue = false; self->input = http::http_response{}; }
			*d = &(self->input);
		}, [self]() //header_cb
		{
			LOGTRACE("header_cb");
			if ( self->input.status_code() == 100 )
				self->http_continue = true;
			else
				self->on_header( std::move( self->input ) );
		}, [self](dstring body) //body
		{
			LOGTRACE("body_cb");
			if ( ! self->http_continue ) self->on_body( std::move ( body ) );
		}, [self](dstring key, dstring val) // trailer
		{
			LOGTRACE("trailer_cb");
			if ( ! self->http_continue )self->on_trailer( std::move( key ), std::move( val ) );
		}, [self]()
		{
			LOGTRACE("completion_cb");
			if ( self->http_continue ) self->on_response_continue();
			else 
			{
				LOGTRACE("on eom cb");
				self->on_eom();
			}
		}, [self]( int err, bool& fatal) // error
		{
			LOGTRACE("Error!");
			switch(err)
			{
				case HPE_UNEXPECTED_CONTENT_LENGTH:
					fatal = false;
					self->codec.ingnore_content_len();
					break;
				default:
					fatal = true;
			}
			self->errcode = INTERNAL_ERROR_LONG(errors::http_error_code::internal_server_error); // do it better
			self->stop();
		});
	
 	auto&& socket_factory = 
		service::locator::socket_pool_factory<socket>().get_socket_factory(0);

	socket_factory->get_socket( req_ref, [self]
		( std::unique_ptr<network::socket_factory<connector_clear::socket>::socket_type>&& st )
		{
			if ( st )
			{
				LOGTRACE("Connection established!");
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
			else {
				LOGTRACE("Something was wrong! 1");
				self->on_error(667);
			}
		}, [self]() mutable
		{

			LOGTRACE("Something was wrong! 2");
			self->on_error(666);
			self->stop();
		} );
	
	close_on_eom = ! req_ref.keepalive();
	
	// Select Http1 Encoder decoder
	dstring msg = codec.encode_header( req_ref );
    std::string msg_str = msg;
    LOGTRACE("Writing to client", msg_str);
	output.write( std::move( msg ) );
	socket_ref = std::move( socket_factory ); // Needed or lifecycle goes nuts
}

std::shared_ptr<connector_clear> connector_clear::make_connector_clear( 
	const http::http_request &req, std::shared_ptr<receiver>& recv ) 
{
	std::shared_ptr<connector_clear> r{ new connector_clear( req, recv ) };
	r->init();
	return r;
}

connector_clear::connector_clear( const http::http_request &req, std::shared_ptr<receiver>& recv ): connector{req, recv}
{
	LOGTRACE("Built!");
}

void connector_clear::send_body( dstring&& body ) noexcept
{
	LOGTRACE(" Called ");
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
	rec->on_header( std::move( r ) );
}
void connector_clear::on_body(dstring&& body) noexcept
{
	rec->on_body( std::move( body ) );
}

void connector_clear::on_trailer(dstring&& key, dstring&& val) noexcept
{
	rec->on_trailer( std::move(key), std::move(val) );
}

void connector_clear::on_eom() noexcept 
{
	LOGTRACE("ON_EOM");
	rec->on_eom();
	if ( close_on_eom )
		stop();
}

void connector_clear::stop() noexcept
{
	LOGTRACE("Stopped!");
	
	// buffer will be stopped on destruction
	rec->stop();
}
void connector_clear::on_error( int error ) noexcept 
{
	rec->on_error( error );
}

connector_clear::~connector_clear()
{
	LOGTRACE("Dead! XXXXXX");
}

}

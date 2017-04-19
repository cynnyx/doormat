#include "connector_clear.h"
#include "communicator/communicator.h"
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
	
	codec.register_callback( [this](http::http_structured_data** d)
		{
			LOGTRACE("started");
			if ( http_continue ) { http_continue = false; input = http::http_response{}; }
			*d = &input;
		}, [this]() //header_cb
		{
			LOGTRACE("header_cb");
			if ( input.status_code() == 100 )
				http_continue = true;
			else
				on_header( std::move( input ) );
		}, [this](dstring body) //body
		{
			LOGTRACE("body_cb");
			if ( ! http_continue ) on_body( std::move ( body ) );
		}, [this](dstring key, dstring val) // trailer
		{
			LOGTRACE("trailer_cb");
			if ( ! http_continue )on_trailer( std::move( key ), std::move( val ) );
		}, [this]()
		{
			LOGTRACE("completion_cb");
			if ( http_continue ) on_response_continue();
			else 
			{
				LOGTRACE("on eom cb");
				on_eom();
			}
		}, [this]( int err, bool& fatal) // error
		{
			LOGTRACE("Error!");
			switch(err)
			{
				case HPE_UNEXPECTED_CONTENT_LENGTH:
					fatal = false;
					codec.ingnore_content_len();
					break;
				default:
					fatal = true;
			}
			errcode = INTERNAL_ERROR_LONG(errors::http_error_code::internal_server_error); // do it better
			stop();
		});
	
 	socket_ref = 
		service::locator::socket_pool_factory<socket>().get_socket_factory(0);

	std::weak_ptr<connector_clear> w_self = self;
	socket_ref->get_socket( req_ref, [w_self]
		( std::unique_ptr<network::socket_factory<connector_clear::socket>::socket_type>&& st )
		{
			if ( w_self.expired() ) return;
			auto self = w_self.lock();
			if ( st )
			{
				LOGTRACE("Connection established!");
				self->output.set_communicator(
					std::unique_ptr<network::communicator<connector_clear::socket>>
					{
						new communicator<>(std::move(st), [w_self](const char *data, size_t size)
						{
							if ( w_self.expired() ) return;
							auto s_self = w_self.lock();
							if ( ! s_self->codec.decode( data, size ) )
							{
								s_self->on_error( INTERNAL_ERROR_LONG( errors::http_error_code::internal_server_error ) );
								s_self->stop();
							}
						}, [w_self] ( errors::error_code errc ) 
						{
							if ( w_self.expired() ) return;
							auto s_self = w_self.lock();
							s_self->on_error( errc );
							s_self->stop();
						}, static_cast<int64_t>(service::locator::configuration().get_board_timeout()))
					} );
			}
			else
			{
				LOGTRACE("Something was wrong! 1");
				self->on_error(667);
				self->stop();
			}
		}, [w_self]() //mutable
		{
			if ( w_self.expired() ) return;
			auto self = w_self.lock();
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
	//socket_ref = std::move( socket_factory ); // Needed or lifecycle goes nuts
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
	LOGERROR("ERROR!");
	rec->on_error( error );
}

connector_clear::~connector_clear()
{
	LOGTRACE("Gone! XXXX");
}

}

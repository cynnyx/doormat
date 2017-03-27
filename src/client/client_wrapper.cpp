#include "client_wrapper.h"

#include "../network/connector_clear.h"
#include "../utils/log_wrapper.h"
#include "../errors/error_codes.h"

namespace client 
{
	
void client_wrapper::cw_receiver::on_eom() noexcept
{
	cw.on_end_of_message();
}

void client_wrapper::cw_receiver::on_body(dstring&& body) noexcept
{
	cw.on_body( std::move( body ) );
}

void client_wrapper::cw_receiver::on_trailer(dstring && key, dstring && val) noexcept
{
	cw.on_trailer( std::move( key ), std::move( val ) );
}

void client_wrapper::cw_receiver::on_header(http::http_response && resp) noexcept
{
	cw.on_header ( std::move( resp ) );
}

void client_wrapper::cw_receiver::on_error(int error) noexcept
{
	cw.on_error( INTERNAL_ERROR_LONG( error ) );
}

void client_wrapper::cw_receiver::stop() noexcept 
{
	// ?
}

void client_wrapper::on_request_preamble(http::http_request && message)
{
	recv.reset( new cw_receiver( *this) );
	if ( ! message.ssl() )
	{
		// this should be another request - that we should take
		connection.reset( new network::connector_clear( message, *recv ) );
	}
	else
	{
		LOGERROR(" SSL Not Implemented for clients yet ");
		on_error( INTERNAL_ERROR_LONG( 501 ) );
	}
}

void client_wrapper::on_request_body(dstring && chunk)
{
	assert(static_cast<bool>(connection));
	connection->send_body( std::move( chunk ) );
}

void client_wrapper::on_request_trailer(dstring && k, dstring && v)
{
	assert(static_cast<bool>(connection));
	connection->send_trailer( std::move( k ), std::move( v ) );
}

void client_wrapper::on_request_canceled(const errors::error_code& err)
{
	assert(static_cast<bool>(connection));
	connection->stop();
}

void client_wrapper::on_request_finished()
{
	assert(static_cast<bool>(connection));
	connection->send_eom();
}

}

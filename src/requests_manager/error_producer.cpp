#include "error_producer.h"
#include "../utils/likely.h"
#include "../utils/log_wrapper.h"
#include "../service_locator/service_locator.h"
#include "../stats/stats_manager.h"
#include "../log/log.h"

#include <chrono>

namespace nodes
{

error_producer::error_producer(req_preamble_fn request_preamble,
	req_chunk_fn request_body,
	req_trailer_fn request_trailer,
	req_canceled_fn request_canceled,
	req_finished_fn request_finished,
	header_callback hcb,
	body_callback bcb,
	trailer_callback tcb,
	end_of_message_callback eomcb,
	error_callback ecb,
	response_continue_callback rccb,
	logging::access_recorder *aclogger): node_interface(std::move(request_preamble), std::move(request_body),
		std::move(request_trailer), std::move(request_canceled),
		std::move(request_finished),
		std::move(hcb), std::move(bcb), std::move(tcb), std::move(eomcb),
		std::move(ecb), std::move(rccb), aclogger), ne(*this) {}


errors::http_error_code error_producer::mapper() const noexcept
{
	switch ( code.code() )
	{
		case 403: return errors::http_error_code::forbidden;
		case 404: return errors::http_error_code::not_found;
		case 405: return errors::http_error_code::method_not_allowed;
		case 408: return errors::http_error_code::request_timeout;
		case 411: return errors::http_error_code::length_required;
		case 414: return errors::http_error_code::uri_too_long;
		case 501: return errors::http_error_code::not_implemented;
		case 502: return errors::http_error_code::bad_gateway;
		case 503: return errors::http_error_code::service_unavailable;
		case 504: return errors::http_error_code::gateway_timeout;
		case 500:
		default:
			return errors::http_error_code::internal_server_error;
	}
}

void error_producer::on_error( const errors::error_code& ec )
{
	if ( client_stream_begun )
		return base::on_error(ec);

	if ( UNLIKELY( bool(efa) ) )
	{
		LOGERROR("Some node invoked on_error() after an on_error() has been triggered!");
		assert(0);
		return;
	}

	// increment failed requests stats
	service::locator::stats_manager().enqueue(stats::failed_req_number, 1);

	code = ec;
	efa = errors::error_factory_async::error_response( ne, static_cast<uint16_t>(mapper()), proto);
}

void error_producer::on_request_body( dstring&& chunk )
{
	if ( LIKELY( ! efa ) ) base::on_request_body( std::move( chunk ) );
}

void error_producer::on_request_trailer(dstring&& k, dstring&& v)
{
	if( LIKELY( ! efa ) ) base::on_request_trailer( std::move( k ), std::move( v ) );
}

void error_producer::on_header(http::http_response&& header)
{
	client_stream_begun = true;
	base::on_header(std::move(header));
}

void error_producer::on_request_preamble( http::http_request&& preamble )
{
	proto = preamble.protocol_version();
	if ( LIKELY( ! efa ) ) base::on_request_preamble( std::move( preamble ) );
}

void error_producer::on_request_finished()
{
	if ( LIKELY( ! efa ) ) base::on_request_finished();
}

}

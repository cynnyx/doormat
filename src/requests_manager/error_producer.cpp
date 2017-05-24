#include "error_producer.h"
#include "../utils/likely.h"
#include "../utils/log_wrapper.h"
#include "../service_locator/service_locator.h"
#include "../stats/stats_manager.h"
#include "../log/log.h"

#include <chrono>

namespace nodes
{

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
        case 415: return errors::http_error_code::unsupported_media_type;
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

    if(!management_enabled) return base::on_error(ec);


    if(efa)
    {
        LOGERROR("Some node invoked on_error() after an on_error() has been triggered!");
        assert(0);
        return;
    }
	service::locator::stats_manager().enqueue(stats::failed_req_number, 1);
	code = ec;
	efa = std::make_unique<errors::error_factory>(mapper(), proto);
    //the creation of the error factory object switches the behaviour of the node, not allowing anymore any information
    //to pass from upstream.
    efa->produce_error_response(
			[this](auto&& res){ static_cast<base*>(this)->on_header(std::move(res)); },
			[this](auto&& data, auto&& len) { static_cast<base*>(this)->on_body(std::move(data), len); },
			[this](auto&& k, auto&& v) { static_cast<base*>(this)->on_trailer(std::move(k), std::move(v)); },
			[this](){ static_cast<base*>(this)->on_end_of_message(); }
    );
}

void error_producer::on_request_body(data_t data , size_t len)
{
	if (!efa) base::on_request_body( std::move( data ), len );
}

void error_producer::on_request_trailer(std::string&& k, std::string&& v)
{
	if(!efa) base::on_request_trailer( std::move( k ), std::move( v ) );
}

void error_producer::on_header(http::http_response&& header)
{
    if(!efa)
    {
        management_enabled = false;
        base::on_header(std::move(header));
    }
}


void error_producer::on_body(data_t data, size_t len)
{
    if(!efa)
    {
		return base::on_body(std::move(data), len);
    }
}


void error_producer::on_trailer(std::string&& k, std::string&& v)
{

    if(!efa)
    {
        return base::on_trailer(std::move(k), std::move(v));
    }
}

void error_producer::on_end_of_message()
{

    if(!efa) {
        return base::on_end_of_message();
    }
}


void error_producer::on_request_preamble( http::http_request&& preamble )
{
	proto = preamble.protocol_version();
	if (!efa) base::on_request_preamble( std::move( preamble ) );
}

void error_producer::on_request_finished()
{
	if (!efa) base::on_request_finished();
}

}

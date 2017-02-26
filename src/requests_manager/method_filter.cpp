#include "method_filter.h"
#include "../utils/likely.h"


namespace nodes
{

void method_filter::on_request_preamble(http::http_request&& preamble)
{
	allowed = is_allowed(preamble.method_code());
	if( LIKELY( allowed ) )
		return base::on_request_preamble(std::move(preamble));

	on_error( INTERNAL_ERROR((std::int32_t) errors::http_error_code::method_not_allowed) );
}

void nodes::method_filter::on_request_body(dstring&& chunk)
{
	if ( LIKELY( allowed ) ) return base::on_request_body(std::move(chunk));
}

void nodes::method_filter::on_request_trailer(dstring&& k, dstring&& v)
{
	if ( LIKELY( allowed ) ) return base::on_request_trailer(std::move(k), std::move(v));
}

void method_filter::on_request_finished()
{
	if ( LIKELY( allowed ) ) return base::on_request_finished();
}

bool method_filter::is_allowed(http_method method)
{
	switch(method)
	{
		//supported methods!
		case HTTP_GET:
		case HTTP_POST:
		case HTTP_OPTIONS:
		case HTTP_PUT:
		case HTTP_HEAD:
		case HTTP_DELETE:
		case HTTP_PATCH:
			return true;
		default:
			return false;
	}
}


} // namespace nodes


#include "size_filter.h"
#include "../service_locator/service_locator.h"
#include "../configuration/configuration_wrapper.h"
#include "../utils/likely.h"

namespace nodes
{

void size_filter::on_request_preamble(http::http_request &&request)
{
	limit = service::locator::configuration().get_max_reqsize();
	allowed = limit == 0 || limit > request.content_len();
	if( LIKELY(allowed) )
		return base::on_request_preamble(std::move(request));
	on_error(INTERNAL_ERROR_LONG(413));
}

void size_filter::on_request_body(dstring&& chunk)
{
	assert(allowed);
	total_message_size += chunk.size();
	allowed = limit == 0 || limit > total_message_size;

	if( LIKELY(allowed) )
		return base::on_request_body(std::move(chunk));
	on_request_canceled(INTERNAL_ERROR_LONG(413));
}

void size_filter::on_request_trailer(dstring&& k, dstring&& v)
{
	assert(allowed);
	total_message_size += k.size()+v.size();
	allowed = limit == 0 || limit > total_message_size;

	if( LIKELY(allowed) )
		return base::on_request_trailer(std::move(k), std::move(v));
	on_request_canceled(INTERNAL_ERROR_LONG(413));
}

void size_filter::on_request_finished()
{
	assert(allowed);
	base::on_request_finished();
}

}

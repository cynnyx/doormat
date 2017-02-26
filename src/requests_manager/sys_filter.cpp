#include "sys_filter.h"

#include "../service_locator/service_locator.h"
#include "../configuration/configuration_wrapper.h"
#include "../utils/likely.h"
#include "../constants.h"

namespace nodes
{

void sys_filter::on_request_preamble( http::http_request&& preamble )
{
	auto&& hostname = preamble.hostname();
	std::string ip = preamble.origin().to_string();

	if( utils::i_starts_with( hostname, sys_subdomain_start() ) && !service::locator::configuration().privileged_ipv4_matcher( ip ) )
	{
		error_code = INTERNAL_ERROR(errors::http_error_code::forbidden);
	}
	else
	{
		bool has_routed_header = preamble.has(http::hf_cyn_dest);
		bool has_routed_header_port = preamble.has(http::hf_cyn_dest_port);

		if (has_routed_header || has_routed_header_port)
		{
			if(!has_routed_header || !has_routed_header_port)
				error_code = INTERNAL_ERROR(errors::http_error_code::unprocessable_entity);
			else if( !service::locator::configuration().privileged_ipv4_matcher( ip ) )
				error_code = INTERNAL_ERROR(errors::http_error_code::forbidden);
		}
	}

	if ( LIKELY( !error_code ) )
		return base::on_request_preamble(std::move(preamble));
	on_error( error_code );
}

void sys_filter::on_request_body(dstring&& chunk)
{
	if ( LIKELY( !error_code ) )
		return base::on_request_body( std::move ( chunk) );
}

void sys_filter::on_request_trailer(dstring&& k, dstring&& v)
{
	if ( LIKELY( !error_code ) )
		return base::on_request_trailer( std::move( k ), std::move( v ) );
}

void sys_filter::on_request_finished()
{
	if ( LIKELY( !error_code ) )
		return base::on_request_finished();
}

}

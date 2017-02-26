#include "interreg_filter.h"
#include "../service_locator/service_locator.h"
#include "../configuration/configuration_wrapper.h"
#include "../errors/error_codes.h"
#include "../utils/utils.h"
#include "../utils/likely.h"
#include "../constants.h"

namespace nodes
{

void interreg_filter::on_request_preamble( http::http_request&& preamble )
{
	const auto& hostname = preamble.hostname();
	if ( utils::i_starts_with( hostname, interreg_subdomain_start() ) )
	{
		std::string ip = preamble.origin().to_string();
		allowed = service::locator::configuration().interreg_ipv4_matcher( ip );
	}

	if ( LIKELY(allowed) ) return base::on_request_preamble(std::move(preamble));

	on_error(INTERNAL_ERROR((std::int32_t) errors::http_error_code::forbidden ));
}

void interreg_filter::on_request_body(dstring&& chunk)
{
	if ( LIKELY(allowed) ) return base::on_request_body( std::move ( chunk) );
}

void interreg_filter::on_request_trailer(dstring&& k, dstring&& v)
{
	if( LIKELY(allowed) ) return base::on_request_trailer( std::move( k ), std::move( v ) );
}

void interreg_filter::on_request_finished()
{
	if ( LIKELY(allowed) ) return base::on_request_finished();
}

}

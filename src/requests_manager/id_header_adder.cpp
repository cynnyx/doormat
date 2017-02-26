#include "id_header_adder.h"
#include "../http/http_commons.h"
#include "../utils/likely.h"
#include "../constants.h"

namespace nodes
{

void id_header_adder::via_header( http::http_structured_data& preamble )
{
	auto via_value = preamble.header( http::hf_via );
	preamble.remove_header( http::hf_via );

	if( via_value )
		via_value.append(", ");

	via_value.append(preamble.protocol())
		.append(" ")
		.append(preamble.hostname());

	preamble.header(http::hf_via, via_value);
}

void id_header_adder::server_header(http::http_structured_data& preamble )
{
#ifdef VERSION
	thread_local static const dstring server{ (project_name() + "-" + version()).c_str() };
#else
	thread_local static const dstring server{ project_name() };
#endif

	preamble.remove_header( http::hf_server );
	preamble.header( http::hf_server, server);
}

void id_header_adder::on_request_preamble ( http::http_request&& preamble )
{
	via_header( preamble );
	server_header( preamble );
	base::on_request_preamble( std::move( preamble ) );
}

void id_header_adder::on_header( http::http_response && preamble )
{
	via_header( preamble );
	server_header( preamble );
	base::on_header( std::move( preamble ) );
}

}

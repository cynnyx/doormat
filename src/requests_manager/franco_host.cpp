#include "franco_host.h"

#include "../service_locator/service_locator.h"
#include "../configuration/configuration_wrapper.h"
#include "../board/abstract_destination_provider.h"
#include "../errors/error_codes.h"
#include "../stats/stats_manager.h"
#include "../utils/log_wrapper.h"
#include "../utils/likely.h"
#include "../constants.h"

namespace nodes
{

void franco_host::handle_get( const http::http_request& req)
{
	http::http_response response;
	response.protocol( req.protocol_version());
	response.status( 200 );
	response.header(http::hf_content_type, http::content_type( http::content_type_t::application_json ) );

	const auto& path = req.path();
	if( utils::icompare(path,"/boards") )
	{
		LOGTRACE("Showing boards");
		const auto& tmp = service::locator::destination_provider().serialize();
		body = dstring{tmp.data(), tmp.size()};
	}
	else
	{
		LOGTRACE("Serving stats");
		const auto& tmp = service::locator::stats_manager().serialize();
		body = dstring{tmp.data(), tmp.size()};
	}

	response.content_len( body.size() );
	on_header(std::move(response));
	on_body( std::move (body) );
	on_end_of_message();
}

void franco_host::handle_options( const http::http_request& req)
{
	http::http_response response;
	response.protocol( req.protocol_version() );
	response.status( 200 );
	response.content_len( 0 );
	response.header(http::hf_allow, "GET,OPTIONS");

	on_header(std::move(response));
	on_end_of_message();
}

void franco_host::handle_post( const http::http_request& req)
{
	auto cyndest = req.header(http::hf_cyn_dest);
	auto cynport = req.header(http::hf_cyn_dest_port);
	if( cyndest && cynport )
	{
		routing::abstract_destination_provider::address address;
		std::string _cyndest = std::string(cyndest); //fixme
		address.ipv6(_cyndest);
		auto portn = address.port();
		cynport.to_integer(portn);
		address.port(portn);
		if( service::locator::destination_provider().disable_destination( address ) )
		{
			http::http_response response;
			response.protocol( req.protocol_version() );
			response.status( 200 );
			response.content_len( 0 );
			on_header(std::move(response));
			return on_end_of_message();
		}
		else
			return on_error( INTERNAL_ERROR( errors::http_error_code::not_found ) );
	}

	return on_error( INTERNAL_ERROR( errors::http_error_code::bad_request ) );
}

void franco_host::on_request_preamble( http::http_request&& preamble )
{
	const auto& hostname = preamble.hostname();
	if( UNLIKELY( utils::i_starts_with( hostname, admin_subdomain_start() ) ) )
	{
		LOGTRACE("Franco is pleased to host!");
		std::string ip = preamble.origin().to_string();

		active = true;
		if ( ! service::locator::configuration().privileged_ipv4_matcher( ip ) )
		{
			on_error(INTERNAL_ERROR(errors::http_error_code::forbidden));
		}
		else
		{
			auto&& method = preamble.method_code();
			switch( method )
			{
				case HTTP_GET:
					handle_get(preamble);
				break;
				case HTTP_OPTIONS:
					handle_options(preamble);
				break;
				case HTTP_POST:
					handle_post(preamble);
				break;
				default:
					on_error(INTERNAL_ERROR(errors::http_error_code::method_not_allowed));
			}
		}
	}
	else base::on_request_preamble(std::move(preamble));
}


void franco_host::on_request_body( dstring&& chunk )
{
	if ( ! active ) base::on_request_body( std::move ( chunk ) );
}

void franco_host::on_request_trailer(dstring&& k, dstring&& v)
{
	if( ! active ) base::on_request_trailer( std::move( k ), std::move( v ) );
}

void franco_host::on_request_finished()
{
	if ( ! active ) base::on_request_finished();
}

}

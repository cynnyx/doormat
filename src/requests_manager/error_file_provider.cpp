#include "error_file_provider.h"

#include "../chain_of_responsibility/node_interface.h"
#include "../configuration/configuration_wrapper.h"
#include "../utils/resource_reader.h"
#include "../errors/error_factory_async.h"
#include "../utils/log_wrapper.h"
#include "../http/http_commons.h"
#include "../service_locator/service_locator.h"
#include "cynnypp/async_fs.hpp"

#include <boost/filesystem.hpp>

static const std::string log_tag_errors = "[errors.* vhost] ";
namespace nodes
{

error_file_provider::error_file_provider(req_preamble_fn request_preamble, req_chunk_fn request_body,
	req_trailer_fn request_trailer, req_canceled_fn request_canceled,
	req_finished_fn request_finished, header_callback  hcb, body_callback bcb,
		trailer_callback tcb, end_of_message_callback eomcb,  error_callback ecb,
		response_continue_callback rcc, logging::access_recorder *aclogger)
		: node_interface(
			std::move(request_preamble), std::move(request_body), std::move(request_trailer),
			std::move(request_canceled), std::move(request_finished),
			std::move(hcb), std::move(bcb), std::move(tcb), std::move(eomcb),
			std::move(ecb), std::move(rcc), aclogger), ne(*this)

{}

bool error_file_provider::valid_method( http_method i_method ) const noexcept
{
	return i_method == http_method::HTTP_GET || i_method == http_method::HTTP_HEAD || http_method::HTTP_OPTIONS;
}

bool error_file_provider::double_dot( const std::string& path )
{
	static const std::string double_dot = "..";
	return path.find(double_dot) != std::string::npos;
}

dstring error_file_provider::path_to_file( const dstring& path_str )
{
	LOGTRACE("ErrorHost file provider ", path_str );

	// empty path and paths containing ".." are considered forbidden
	if ( path_str.empty() || double_dot( path_str ) ) return "";

	auto res_dir = service::locator::configuration().get_path();
	if( res_dir.empty() )
		throw std::logic_error("For security reasons, resource paths should not be empty");

	// we want to be sure that we are returning a path *inside* res_dir
	if( res_dir.back() != '/' && path_str.front() != '/')
		return "";

	res_dir.append(path_str);
	return dstring{res_dir.data(), res_dir.size()};
}

void error_file_provider::on_request_preamble(http::http_request&& preamble)
{
	auto&& hostname = preamble.hostname();
	std::string errorHost = service::locator::configuration().get_error_host();
	errorHost = utils::hostname_from_url( errorHost );

	active = utils::icompare( std::string(hostname), errorHost );

	if ( ! active )
		return base::on_request_preamble( std::move( preamble ) );

	LOGTRACE("Active now!");
	http::proto_version proto = preamble.protocol_version();
	http_method i_method = preamble.method_code();

	if ( ! valid_method( i_method ) )
	{
		LOGTRACE( "Called errors with a not allowed method!" );
		return on_error(INTERNAL_ERROR(errors::http_error_code::method_not_allowed));
	}

	if(i_method == HTTP_OPTIONS)
	{
		http::http_response response;
		response.protocol( proto );
		response.status( 200 );
		response.content_len( 0 );
		response.header(http::hf_allow, "GET,HEAD,OPTIONS");
		on_header(std::move(response));
		return on_end_of_message();
	}

	std::string filename = path_to_file( preamble.path() );
	if ( filename.empty() )
	{
		LOGTRACE("Forbidden!");
		return on_error(INTERNAL_ERROR(errors::http_error_code::forbidden));
	}

	boost::system::error_code ec;
	ec.clear();
	std::size_t clen = boost::filesystem::file_size( filename, ec );
	LOGTRACE( "Clen: ", filename, " ", clen );

	if ( ec )
	{
		LOGTRACE("File not found in error host");
		return on_error(INTERNAL_ERROR(errors::http_error_code::not_found));
	}

	http::http_response headers;
	headers.protocol( proto );
	headers.status(200);
	headers.content_len( clen );
	auto type = http::content_type( filename );
	headers.header(http::hf_content_type, http::content_type(type));
	headers.header(http::hf_content_type, http::hv_charset_utf8);
	base::on_header(std::move(headers));

	if ( i_method == http_method::HTTP_GET )
	{
		auto reader = std::make_shared<utils::resource_reader>( filename,
			[this, proto](const utils::resource_reader::error_t& ecc, utils::resource_reader::buffer_t buf)
			{
				LOGTRACE("Callback of resource reader!");
				if ( ecc && ecc != utils::resource_reader::error_t::end_of_file )
				{
					LOGERROR(log_tag_errors , ecc.what() );
					ne.on_error( INTERNAL_ERROR_LONG(errors::http_error_code::internal_server_error));
				}

				dstring body{reinterpret_cast<const char *>(buf.data()), buf.size()};

				ne.on_body( std::move( body ) );
			},
			[this, clen] ( const error_t& ec, size_t total_bytes )
			{
				if ( clen != total_bytes ) LOGERROR("Sent less bytes than Content-Length");
				LOGTRACE("Closing error host file");
				ne.on_end_of_message();
			}
		);
		reader->start();
	}
}


void error_file_provider::on_request_body(dstring&& chunk)
{
	if ( active )
	{
		LOGTRACE( "We should not receive a body" );
		return;
	}
	base::on_request_body( std::move( chunk ) );
}

void error_file_provider::on_request_trailer(dstring&& k, dstring&& v)
{
	if ( active )
	{
		LOGTRACE( "We should not receive trailers" );
		return;
	}
	base::on_request_trailer( std::move( k ), std::move( v ) );
}

void error_file_provider:: on_request_finished()
{
	if ( active ) return;

	base::on_request_finished();
}

}

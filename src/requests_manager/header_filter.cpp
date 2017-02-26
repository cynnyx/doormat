#include "header_filter.h"
#include "../utils/likely.h"

namespace nodes
{

void header_filter::on_request_preamble(http::http_request&& preamble)
{
	auto not_implemented_predicate = [this, &preamble](const http::http_request::header_t& h)
	{
		const auto &key = h.first;
		const auto &value = h.second;

		if( preamble.channel() == http::proto_version::HTTP20
				&& key == "expect" && utils::icompare(value, "100-Continue") )
			return true;

		if ( key == "transfer-encoding" && !utils::icompare(value, "chunked") )
			return true;

		return false;
	};

	//MAX_FORWARD
	auto&& method = preamble.method_code();
	if( method == HTTP_OPTIONS || method == HTTP_TRACE )
	{
		dstring mf = preamble.header( http::hf_maxforward );
		if( mf.size())
		{
			uint value;
			mf.to_integer(value);
			if( value == 0 )
			{
				http::http_response response;
				response.protocol( preamble.protocol_version() );
				response.status( 200 );
				response.content_len( 0 );
				response.header(http::hf_allow, "GET, HEAD, POST, OPTIONS, PUT, DELETE, PATCH");
				on_header(std::move(response));
				return on_end_of_message();
			}

			value--;
			preamble.remove_header(http::hf_maxforward);
			preamble.header(http::hf_maxforward, dstring::to_string(value));
		}
	}

	rejected = preamble.has(std::move(not_implemented_predicate));

	if( UNLIKELY(rejected) )
		return on_error(INTERNAL_ERROR(errors::http_error_code::not_implemented));

	auto remove_predicate = [this](const http::http_request::header_t& h)
	{
		const auto &key = h.first;
		if( key == "connection" || key == "keep-alive" || key == "public"
				|| key == "proxy-authenticate"|| key == "transfer-encoding" || key == "upgrade" )
		{
			return true;
		}

		return false;
	};
	preamble.filter(std::move(remove_predicate));

	base::on_request_preamble(std::move(preamble));
}

void header_filter::on_request_body(dstring&& chunk)
{
	if ( LIKELY( ! rejected ) ) return base::on_request_body(std::move(chunk));
}

void header_filter::on_request_trailer(dstring&& k, dstring&& v)
{
	if ( LIKELY( ! rejected ) ) return base::on_request_trailer(std::move(k), std::move(v));
}

void header_filter::on_request_finished()
{
	if ( LIKELY( ! rejected ) ) return base::on_request_finished();
}

} // namespace nodes

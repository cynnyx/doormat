#include "http_codec.h"
#include "http_codec_impl.h"

#include "../utils/utils.h"
#include "../utils/log_wrapper.h"

using namespace std;
using namespace utils;

namespace http
{

http_codec::http_codec()
	: codec_impl(new impl())
{}

http_codec::~http_codec()
{}

dstring http_codec::encode_body(const dstring& data)
{
	assert(_encoder_state == encoder_state::HEADER||_encoder_state == encoder_state::BODY);
	_encoder_state = encoder_state::BODY;

	if(data.empty())
		return {};

	dstring msg;
	if(_chunked)
		msg.append(from<size_t>(data.size()))
				.append(http::crlf)
				.append(data)
				.append(http::crlf);
	else
		msg.append(data);

	return msg;
}

dstring http_codec::encode_trailer(const dstring& key, const dstring& data)
{
	assert(_encoder_state == encoder_state::BODY||_encoder_state == encoder_state::TRAILER);
	_encoder_state = encoder_state::TRAILER;

	dstring msg;
	msg.append(http::transfer_chunked_termination)
		.append(key)
		.append(http::colon_space)
		.append(data)
		.append(http::crlf);
	return msg;
}

dstring http_codec::encode_eom()
{
	assert(_encoder_state != encoder_state::ZERO);
	dstring msg;
	if(_chunked)
	{
		switch(_encoder_state)
		{
			case encoder_state::HEADER:
			case encoder_state::BODY:
				msg.append(http::transfer_chunked_termination);
			case encoder_state::TRAILER:
				msg.append(http::crlf);
			default: break;
		}
	}
	_encoder_state = encoder_state::ZERO;
	return msg;
}

//Caller need to register its own cb
void http_codec::register_callback(const structured_cb& begin,const void_cb& header,
	const stream_cb& body, const trailer_cb& trailer, const void_cb& completion,
	const error_cb& error ) noexcept
{
	codec_impl->register_callback(begin,header,body, trailer,completion,error);
}

bool http_codec::decode(const char* data, size_t len) noexcept
{
	size_t b{0};
	while( len > b )
	{
		b += codec_impl->decode(data+b, len - b);

		if( codec_impl->on_parser_error() != HPE_OK )
			return false;

		if( _skip_next_header )
		{
			_skip_next_header = false;
			if( !codec_impl->headers_done() )
			{
				size_t c = 0;
				for(; c < len; ++c)
				{
					if( data+c == http::crlf )
						break;
				}
				b = c + 1;
			}
		}

		if( _ignore_content_len )
		{
			_ignore_content_len = false;
			codec_impl->ignore_content_len();
		}
	}

	return true;
}

int http_codec::on_message_begin(http_parser* parser)
{
	auto impl = static_cast<http_codec::impl*>(parser->data);
	return impl->on_message_begin();
}

int http_codec::on_url (http_parser* parser, const char *at, size_t length)
{
	auto impl = static_cast<http_codec::impl*>(parser->data);
	return impl->on_url( at, length );
}

int http_codec::on_status (http_parser* parser, const char* at, size_t length)
{
	auto impl = static_cast<http_codec::impl*>(parser->data);
	return impl->on_status(at,length);
}

int http_codec::on_header_field (http_parser* parser, const char *at, size_t length)
{
	auto impl = static_cast<http_codec::impl*>(parser->data);
	if ( !impl->headers_done() )
		return impl->on_header_field( at, length );
	else
		return impl->on_trailer_field( at, length );
}

int http_codec::on_header_value (http_parser* parser, const char *at, size_t length)
{
	auto impl = static_cast<http_codec::impl*>(parser->data);
	if ( !impl->headers_done() )
		return impl->on_header_value( at, length );
	else
		return impl->on_trailer_value( at, length );
}

int http_codec::on_headers_complete(http_parser* parser)
{
	auto impl = static_cast<http_codec::impl*>(parser->data);
	return impl->on_headers_complete();
}

int http_codec::on_body(http_parser* parser, const char *at, size_t length)
{
	auto impl = static_cast<http_codec::impl*>(parser->data);
	return impl->on_body(at,length);
}

int http_codec::on_chunk_header(http_parser* parser)
{
	auto impl = static_cast<http_codec::impl*>(parser->data);
	return impl->on_chunk_header();
}

int http_codec::on_chunk_complete(http_parser* parser)
{
	auto impl = static_cast<http_codec::impl*>(parser->data);
	return impl->on_chunk_complete();
}

int http_codec::on_message_complete(http_parser* parser)
{
	auto impl = static_cast<http_codec::impl*>(parser->data);
	return impl->on_message_complete();
}

}//namespace http

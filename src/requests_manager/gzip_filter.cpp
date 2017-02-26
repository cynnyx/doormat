#include "gzip_filter.h"
#include "../service_locator/service_locator.h"
#include "../configuration/configuration_wrapper.h"
#include "../errors/error_codes.h"
#include "../utils/likely.h"
#include "../utils/log_wrapper.h"

#define DEFAULT_MEM_LEVEL 8
#define GZIP_ENCODING 16
#define OUT_CHUNK_SIZE 1024

namespace nodes
{
	gzip_filter::gzip_filter(req_preamble_fn request_preamble, req_chunk_fn request_body,
	req_trailer_fn request_trailer, req_canceled_fn request_canceled,
	req_finished_fn request_finished, header_callback hcb, body_callback bcb,
	trailer_callback tcb, end_of_message_callback eomcb, error_callback ecb,
	response_continue_callback rccb, logging::access_recorder *aclogger)
		: node_interface(
			std::move(request_preamble), std::move(request_body),
			std::move(request_trailer), std::move(request_canceled),
			std::move(request_finished),
			std::move(hcb), std::move(bcb), std::move(tcb), std::move(eomcb),
			std::move(ecb), std::move(rccb), aclogger)
		, _dsf{OUT_CHUNK_SIZE}
{
	auto&& conf = service::locator::configuration();
	enabled = conf.get_compression_enabled();
	level = conf.get_compression_level();
	min_size = conf.get_compression_minsize();
	LOGTRACE(this,"enabled: ", enabled, " level: ", (unsigned)level," min_size: ", min_size);
}

gzip_filter::~gzip_filter()
{
	if(_stream)
	{
		const auto& rv = deflateEnd( _stream.get() );
		if( rv != Z_OK)
			LOGERROR("compressor::~compressor - Failure on deflateEnd():", _stream->msg );
	}
}

bool gzip_filter::parse_accepted_encoding(const dstring& val)
{
	if( val == "*" )
		return true;

	size_t pos{0};
	size_t offset{0};
	while( pos != std::string::npos )
	{
		offset = val.find(',', pos);

		accept_encoding_item aei{val.substr(pos,offset)};
		if( aei.priority > 0 && aei.name == http::hv_gzip )
			return true;

		pos = offset;
	}
	return false;
}

bool gzip_filter::init_compressor()
{
	_stream.reset( new z_stream );
	_stream->zalloc = Z_NULL;
	_stream->zfree = Z_NULL;
	_stream->opaque = Z_NULL;
	_stream->next_in = 0;
	_stream->avail_in = 0;
	_stream->next_out = 0;
	_stream->avail_out = 0;
	auto rv = deflateInit2( _stream.get(), level, Z_DEFLATED, MAX_WBITS|GZIP_ENCODING, DEFAULT_MEM_LEVEL, Z_DEFAULT_STRATEGY);
	if (rv != Z_OK)
	{
		LOGERROR("init_compressor failed: ", _stream->msg);
		return false;
	}

	return true;
}

bool gzip_filter::compress(int fm, std::function<void(dstring&)> handler)
{
	int rv{Z_OK};
	while( rv == Z_OK )
	{
		if( !_stream->avail_in && !_in_buf.empty() )
		{
			_stream->avail_in = _in_buf.front().size();
			_stream->next_in = (Bytef*)_in_buf.front().cdata();
		}

		if( _stream->avail_out == 0 )
		{
			if( _stream->total_out && handler )
			{
				auto d = _dsf.create_dstring();
				handler(d);
			}

			_stream->avail_out = _dsf.max_size();
			_stream->next_out = (Bytef*)_dsf.data();
		}

		rv = deflate(_stream.get(), fm);

		//if any input has already been digested, drop it
		if( !_stream->avail_in && !_in_buf.empty() )
			_in_buf.pop();

		if( rv == Z_STREAM_END )
		{
			assert(fm == Z_FINISH && _in_buf.empty());
			const auto& len = std::distance((Bytef*) _dsf.data(), _stream->next_out);
			if( handler )
			{
				auto d = _dsf.create_dstring(len);
				handler(d);
			}
		}
		else if( rv == Z_STREAM_ERROR )
		{
			LOGERROR("stream error: ", _stream->msg);
			return false;
		}
	}
	return true;
}

void gzip_filter::on_request_preamble(http::http_request &&request)
{
	if( enabled )
		active = request.has(http::hf_accept_encoding, [this](const dstring& v){ return parse_accepted_encoding(v);});
	return base::on_request_preamble(std::move(request));
}

void gzip_filter::on_request_trailer(dstring&& k, dstring&& v)
{
	if( enabled && !active && utils::icompare(k,http::hf_accept_encoding) )
		active = parse_accepted_encoding(v);
	return base::on_request_trailer(std::move(k), std::move(v));
}

void gzip_filter::on_header(http::http_response&& h)
{
	if( UNLIKELY( active ) )
	{
		const auto& mimetype = h.header(http::hf_content_type);
		bool chunked = h.chunked();
		bool mime_match = service::locator::configuration().comp_mime_matcher( mimetype );
		bool size_match = chunked || h.content_len() >= min_size;

		LOGTRACE(this," mime_match: ", mime_match, "size_match: ", size_match );

		if( mime_match && size_match )
		{
			init_compressor();
			h.header(http::hf_content_encoding, http::hv_gzip);
			if( !chunked )
			{
				//Need to compute len before forwarding
				res.reset(h);
				return;
			}
		}
		else
			active = false;
	}
	base::on_header( std::move(h) );
}

void gzip_filter::on_body(dstring&& b)
{
	if( LIKELY( ! active ) )
		//No compression
		return base::on_body( std::move( b ) );

	_in_buf.push(std::move(b));
	if( !res )
	{
		auto fn = [this](dstring& d){base::on_body(std::move(d));};
		if( ! compress(Z_NO_FLUSH, fn ) )
			return on_error( INTERNAL_ERROR(errors::http_error_code::internal_server_error) );
	}
}

void gzip_filter::on_end_of_message()
{
	if( UNLIKELY( active ) )
	{
		if( !res )
		{
			//Chunked
			auto fn = [this](dstring& d){base::on_body(std::move(d));};

			if( ! compress(Z_FINISH, fn ) )
				return on_error( INTERNAL_ERROR(errors::http_error_code::internal_server_error) );
		}
		else
		{
			//Not chunked
			std::queue<dstring> out;
			auto fn = [this, &out](dstring& d){out.push(std::move(d));};

			if( !compress( Z_FINISH, fn ) )
				return on_error( INTERNAL_ERROR(errors::http_error_code::internal_server_error) );

			res->content_len(_stream->total_out);
			base::on_header(std::move(res.get()));

			while( !out.empty() )
			{
				base::on_body(std::move(out.front()));
				out.pop();
			}
		}
	}
	return base::on_end_of_message();
}

}


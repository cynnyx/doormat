#pragma once

#include <string>
#include <utility>

#include "http_codec.h"
#include "http_request.h"
#include "http_response.h"

#include "../utils/utils.h"
#include "../utils/likely.h"
#include "../utils/log_wrapper.h"
#include "../http_parser/http_parser.h"

namespace http
{

/**
 * @note Static callbacks lands here at the end.
 */
class http_codec::impl
{
	http::http_structured_data* _data{nullptr};
	proto_version _version{proto_version::UNSET};

	bool _ignore{false};
	bool _connected{false};
	bool _got_header{false};
	bool _headers_completed{false};

	//NOTE: this limit is hardcoded to today defacto standard (nginx supports up to 8kb URI)
	//It would be nice to join this check with a more general check on overall header size
	//handled directly by buffer
	const uint16_t max_uri_len{8192};

	dstring _urline;
	dstring _status;
	dstring _key;
	dstring _value;

	http_parser _parser;
	http_parser_settings _parser_settings;
	http_parser_url _url;

	http::http_codec::structured_cb _scb;
	http::http_codec::void_cb _hcb;
	http::http_codec::stream_cb _bcb;
	http::http_codec::trailer_cb _tcb;
	http::http_codec::void_cb _ccb;
	http::http_codec::error_cb _fcb;

	void process_current_header()
	{
		if(!_got_header)
			return;

		auto found = _value.find(',');
		if(_key == "date" || found == std::string::npos)
			_data->header(std::move(_key), std::move(_value));
		else
		{
			size_t beg{};
			do
			{
				while(*(_value.cdata() + beg) == ' ') ++beg;
				while(*(_value.cdata() - 1) == ' ') --found;

				if(found >= _value.size() || found == 0)
				{
					_data->header(_key, _value.substr(beg));
					break;
				}

				// from here on, 0 < found < _value.size()
				_data->header(_key, _value.substr(beg, found - beg));
				beg = found + 1;
				found = _value.find(',', beg);
			}
			while(true);
		}

		_got_header = false;
		_key = dstring{true};
		_value = {};
	}

public:
	impl() noexcept
		: _key{true}
	{
		reset();
	}

	void reset() noexcept
	{
		_data=nullptr;
		_ignore = false;
		_connected = false;
		_got_header = false;
		_headers_completed=false;

		_urline = dstring{};
		_status = dstring{};
		_value = dstring{};
		_key = dstring{true};

		http_parser_init(&_parser, HTTP_BOTH);
		_parser.data = this;

		http_parser_settings_init(&_parser_settings);
		_parser_settings.on_message_begin = &http_codec::on_message_begin;
		_parser_settings.on_url = &http_codec::on_url;
		_parser_settings.on_status = &http_codec::on_status;
		_parser_settings.on_header_field = &http_codec::on_header_field;
		_parser_settings.on_header_value = &http_codec::on_header_value;
		_parser_settings.on_headers_complete = &http_codec::on_headers_complete;
		_parser_settings.on_body = &http_codec::on_body;
		_parser_settings.on_chunk_header = &http_codec::on_chunk_header;
		_parser_settings.on_chunk_complete = &http_codec::on_chunk_complete;
		_parser_settings.on_message_complete = &http_codec::on_message_complete;

		http_parser_url_init(&_url);
	}

	void register_callback(const structured_cb& begin,const void_cb& header,
		const stream_cb& body, const trailer_cb& trailer, const void_cb& completion,
		const error_cb& error) noexcept
	{
		_scb=begin;
		_hcb=header;
		_bcb=body;
		_tcb=trailer;
		_ccb=completion;
		_fcb=error;
	}

	void version( const proto_version& ver ) noexcept { _version = ver; }
	proto_version version() const noexcept { return _version; }
	bool headers_done() const { return _headers_completed; }

	void ignore_content_len()
	{
		//This has been provided to emulate NGINX behavior
		// so to ignore UNEXPECTED CONTENT LENGTH
		// THIS IS NOT HTTP COMPLIANT - USE CAREFULLY
		if( _parser.flags & F_CHUNKED )
		{
			_parser.flags &= ~F_CONTENTLENGTH;
			_parser.content_length = 0;
		}
	}

	int decode(const char * const bytes, size_t size) noexcept
	{
		return http_parser_execute(&_parser, &_parser_settings, bytes, size);
	}

	int on_message_begin() noexcept
	{
		_scb( &_data );
		assert(_data != nullptr);
		return 0;
	}

	int on_url(const char *at, size_t len) noexcept
	{
		_urline.append(at, len);
		if( _urline.size()> max_uri_len )
			return 1;

		auto d = static_cast<http::http_request *>(this->_data);
		if(http_parser_parse_url(at, len, _connected, &_url) == 0)
		{
			d->schema(_urline.substr(_url.field_data[UF_SCHEMA].off,_url.field_data[UF_SCHEMA].len));
			d->urihost(_urline.substr(_url.field_data[UF_HOST].off,_url.field_data[UF_HOST].len));
			d->port(_urline.substr(_url.field_data[UF_PORT].off,_url.field_data[UF_PORT].len));
			d->path(_urline.substr(_url.field_data[UF_PATH].off,_url.field_data[UF_PATH].len));
			d->query(_urline.substr(_url.field_data[UF_QUERY].off,_url.field_data[UF_QUERY].len));
			d->fragment(_urline.substr(_url.field_data[UF_FRAGMENT].off,_url.field_data[UF_FRAGMENT].len));
			d->userinfo(_urline.substr(_url.field_data[UF_USERINFO].off,_url.field_data[UF_USERINFO].len));
		}
		_connected = true;
		return 0;
	}

	int on_status(const char* at, size_t len) noexcept
	{
		assert(_data->type() == typeid(http::http_response));
		_status.append(at,len);
		http::http_response* d = static_cast<http::http_response *>(_data);
		d->status(_parser.status_code,_status);
		return 0;
	}

	int on_header_field( const char *at, size_t len ) noexcept
	{
		assert( _headers_completed == false );
		process_current_header();

		_key.append(at,len);
		return 0;
	}

	int on_header_value(const char *at, size_t len) noexcept
	{
		assert( _headers_completed == false );
		_got_header = true;
		_value.append(at,len);
		return 0;
	}

	int on_headers_complete() noexcept
	{
		// This codec supports HTTP/1.x only
		assert( _parser.http_major == 1 );

		//Set protocol
		if( _version == proto_version::UNSET )
			_version = (_parser.http_minor == 1)? proto_version::HTTP11:proto_version::HTTP10;
		_data->protocol(_version);

		//Set last header
		process_current_header();

		//Set method (request only)
		if( _data->type() == typeid(http::http_request) )
		{
			auto req = static_cast<http::http_request *>(_data);
			req->method((http_method)(_parser.method));
			if(req->urihost().empty())
			{
				auto&& host = req->header("host");
				auto pos = host.find(':');
				if(pos == std::string::npos)
					req->urihost(host);
				else
				{
					req->urihost(dstring{host.cdata(), pos++});
					req->port(dstring{host.cdata() + pos, host.size() - pos});
				}
			}
		}

		//Set persistency
		_data->keepalive(http_should_keep_alive(&_parser));

		//Notify cb
		_hcb();

		//
		_headers_completed=true;
		return 0;

		//TODO: return 1 to force skip body if needed
	}

	int on_trailer_field(const char *at, size_t len) noexcept
	{
		if( _got_header )
		{
			_tcb(std::move(_key), std::move(_value));
			_got_header = false;
			_key = dstring{true};
			_value = {};
		}

		_key.append(at,len);
		return 0;
	}

	int on_trailer_value(const char *at, size_t len) noexcept
	{
		//In case of splitted trailer we're not granted this cb
		//will be called only upon completion.
		//Apparently parser does not offer a criteria to check
		//trailer completness.
		//_tcb has to be called on next 'on_trailer_field' or
		//'on_eom' invocation.

		_got_header = true;
		_value.append(at,len);
		return 0;
	}

	int on_body(const char *at, size_t len) noexcept
	{
		if(!_ignore)
			_bcb(dstring{at,len});
		return 0;
	}

	int on_chunk_header() noexcept
	{
		if(http_body_is_final(&_parser))
			_ignore = true;
		return 0;
	}

	int on_chunk_complete() noexcept
	{
		if(http_body_is_final(&_parser))
			_ignore = true;
		return 0;
	}

	int on_message_complete( ) noexcept
	{
		if( _got_header )
		{
			_tcb(std::move(_key), std::move(_value));
			_got_header = false;
			_key = dstring{true};
			_value = {};
		}

		//Apparently keepalive can be instructed in trailers,
		//but that's not used in cynny, so for now NOT SUPPORTED
		//_keepalive = http_should_keep_alive(&_parser);
		_ccb();
		reset();
		return 0;
	}

	int on_parser_error( ) noexcept
	{
		auto err = HTTP_PARSER_ERRNO(&_parser);
		if( err != HPE_OK )
		{
			LOGERROR( "http_codec_impl ", this, ":", http_errno_name(err), "(", err, ") ", http_errno_description(err));
			bool fatal{true};
			_fcb(err, fatal);

			if( LIKELY(fatal) )
				reset();
			else
			{
				//Reset error
				err = HPE_OK;
				_parser.http_errno = HPE_OK;
			}
		}
		return err;
	}

};

}

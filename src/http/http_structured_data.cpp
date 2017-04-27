#include "http_structured_data.h"
#include "http_commons.h"

#include <string>
#include <ctime>

static dstring empty;

namespace http
{

http_structured_data::http_structured_data(std::type_index type)
	: _type{type}
{
	//x-cyn-date set to default.
	header(http::hf_content_len, "0");
	header(http::hf_connection, http::hv_connection_close);
}

bool http_structured_data::operator== ( const http::http_structured_data& other ) const
{
	return has_same_headers ( other ) && _protocol == other._protocol
		&& _channel == other._channel && _chunked == other._chunked
		&& _keepalive == other._keepalive && _content_len == other._content_len;
}

bool http_structured_data::has_same_headers ( const http::http_structured_data& other ) const
{
	bool rv{false};
	if( _headers.size() == other._headers.size() )
	{
		rv = std::equal( other._headers.cbegin(), other._headers.cend(), _headers.cbegin(),
			[](const header_t& a, const header_t& b){ return a == b; });
	}
	return rv;
}

void http_structured_data::header( const dstring& key, const dstring& value ) noexcept
{
	if( key == http::hf_connection )
	{
		remove_header(key);
	}
	else if( key == http::hf_content_len )
	{
		value.to_integer(_content_len);
		remove_header(key);
	}
	else if( key == http::hf_transfer_encoding && utils::icompare(value,"chunked"))
	{
		_chunked = true;
		remove_header(http::hf_transfer_encoding);
		remove_header(http::hf_content_len);
	}

	_headers.insert( std::make_pair( key, value ) );
}

const dstring& http_structured_data::header( const dstring& key ) const noexcept
{
	auto&& element = _headers.find( key );
	if ( element != _headers.end() )
		return element->second;
	return empty;
}

std::list<dstring> http_structured_data::headers( const dstring& key ) const noexcept
{
	auto&& span = _headers.equal_range(key);
	std::list<dstring> hl;
	std::for_each( span.first, span.second, [&hl](const header_t& h){hl.emplace_back(h.second);});
	return hl;
}

void http_structured_data::remove_header(const dstring& key) noexcept
{
	_headers.erase( key );
}

bool http_structured_data::has( const dstring& key ) const noexcept
{
	return header( key ).size();
}

bool http_structured_data::has(const dstring& key, const dstring& val ) const noexcept
{
	return has( key, [&val](const dstring& v){ return v == val;} );
}

bool http_structured_data::has(const dstring& key, std::function<bool(const dstring&)> pred ) const noexcept
{
	auto&& hl = _headers.equal_range( key );
	auto&& it = std::find_if( hl.first, hl.second, [&pred](const header_t& h){ return pred(h.second);});
	return it != hl.second;
}

bool http_structured_data::has( std::function<bool ( const header_t& ) > predicate ) const noexcept
{
	return std::find_if(_headers.cbegin(), _headers.cend(), predicate ) != _headers.cend();
}

void http_structured_data::filter( std::function<bool ( const header_t& ) >  predicate )
{
	auto&& it = _headers.begin();
	while ( it != _headers.end() )
	{
		if ( predicate ( *it ) )
			it = _headers.erase ( it );
		else
			++it;
	}
}


void http_structured_data::chunked(bool val) noexcept
{
	if( val != _chunked )
	{
		if( val )
			header(http::hf_transfer_encoding, hv_chunked);
		else
			remove_header(http::hf_transfer_encoding);

		_chunked = val;
	}
}

void http_structured_data::keepalive(bool val) noexcept
{
	_keepalive = val;
	_default_keepalive = false;
	header(http::hf_connection, val ? http::hv_keepalive : http::hv_connection_close);
}

void http_structured_data::content_len(const size_t& val) noexcept
{
	assert(!_chunked || val == 0);
	header( http::hf_content_len, dstring::to_string(val) );
}

void http_structured_data::protocol ( const proto_version& val ) noexcept
{
	if ( _protocol != val )
	{
		_protocol = val;
		if ( _default_keepalive )
		{
			remove_header( http::hf_connection );
			switch ( val )
			{
				case proto_version::HTTP11:
					_keepalive = true;
				break;
				case proto_version::HTTP10:
					header(http::hf_connection, http::hv_connection_close);
					_keepalive = false;
				break;
				default:
					//http2 protocol does not support explicit keepalive
				break;
			}
		}
	}
}

dstring proto_to_string( proto_version pv ) noexcept
{
	switch ( pv )
	{
		case proto_version::HTTP10:
			return http::http10;
		case proto_version::HTTP11:
			return http::http11;
		case proto_version::HTTP20:
			return http::http20;
		default:
			return empty;
	}
}

dstring http_structured_data::protocol() const noexcept
{
	return proto_to_string( _protocol );
}

dstring http_structured_data::serialize() const noexcept
{
	dstring msg;
	const dstring* last_key = nullptr;
	bool not_first{false};
	for(const auto& h : _headers)
	{
		if(not_first && h.first == *last_key)
		{
			if(h.first == "cookie")
				msg.append(http::semicolon_space);
			else
				msg.append(http::comma_space);
		} else
		{
			if(not_first)
				msg.append(http::crlf);
			last_key = &(h.first);
			msg.append(h.first);
			msg.append(http::colon_space);
		}

		//FIXME: exclude invalid chunks from being serialized
		if(h.second)
			msg.append(h.second);
		not_first=true;
	}
	assert(last_key);
	if(!last_key->empty())
		msg.append(http::crlf);
	msg.append(http::crlf);
	return msg;
}

}

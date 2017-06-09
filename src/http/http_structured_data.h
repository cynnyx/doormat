#pragma once

#include <boost/asio/ip/address.hpp>

#include <limits>
#include <memory>
#include <typeindex>

#include "http_commons.h"

#include "../utils/doormat_types.h"
#include "../utils/utils.h"
#include "../http_parser/http_parser.h"

namespace http
{

/** Headers needed, whatever happens:
 * . Connection
 * . Content-Length / Chunked Enconding
 * */
class http_structured_data
{
public:
	using headers_map = std::multimap<std::string, std::string>;
private:
	std::type_index _type;

	proto_version _protocol {proto_version::UNSET};
	proto_version _channel {proto_version::UNSET};
	std::multimap<std::string, std::string> _headers;

	bool _chunked {false};
	bool _keepalive {false};
	bool _default_keepalive {true};
	size_t _content_len {0};

	//origin
	boost::asio::ip::address _origin;

	// These headers must be populated in client wrapper, where the destination is known!
	std::vector<std::string> destination_headers;

protected:
	http_structured_data ( std::type_index type );
	http_structured_data ( const http_structured_data& c ) = default;
	http_structured_data ( http_structured_data&& c ) = default;
	http_structured_data& operator= ( const http_structured_data& c ) = default;
	http_structured_data& operator= ( http_structured_data&& c ) = default;

	bool has_same_headers ( const http_structured_data& other ) const;

public:
	using header_t = typename headers_map::value_type;

	std::type_index type() const { return _type; }

	bool operator== ( const http_structured_data& other ) const;
	bool operator!= ( const http_structured_data& other ) const { return !operator== ( other ); }

	// Better make it protected and not virtual
	virtual ~http_structured_data() noexcept = default;

	void header( const std::string& key, const std::string& value ) noexcept;
	void remove_header( const std::string& key ) noexcept;

	// I feel lucky, I'd take only the first hit
	const std::string& header( const std::string& key ) const noexcept;
	std::list<std::string> headers( const std::string& key ) const noexcept;

	// HTTP2ng needs this
	headers_map headers() const;
	std::size_t headers_count() const noexcept { return _headers.size(); }

	bool has( const std::string& ) const noexcept;
	bool has( std::function<bool ( const header_t& ) > ) const noexcept;
	bool has( const std::string& , const std::string& ) const noexcept;
	bool has( const std::string& , std::function<bool(const std::string&)> ) const noexcept;
	void filter ( std::function< bool ( const header_t& ) > );

	bool chunked() const noexcept { return _chunked; }
	bool keepalive() const noexcept { return _keepalive; }
	size_t content_len() const noexcept { return _content_len; }
	proto_version protocol_version() const noexcept { return _protocol; }
	proto_version channel() const noexcept { return _channel; }
	boost::asio::ip::address origin() const noexcept { return _origin; }
	std::string protocol() const noexcept;
	const std::string& date() const noexcept { return header( http::hf_date ); }
	const std::string& hostname() const noexcept { return header( http::hf_host ); }

	void chunked ( bool val ) noexcept;
	void keepalive ( bool val ) noexcept;
	void content_len ( const size_t& val ) noexcept;
	void protocol ( const proto_version& val ) noexcept;
	void channel ( const proto_version& val ) noexcept { _channel = val; }
	void hostname ( const std::string& val ) noexcept { header(http::hf_host, val );}
	void date ( const std::string& val ) noexcept { remove_header(http::hf_date); header ( http::hf_date, val ); }
	void origin ( const boost::asio::ip::address& a ) { _origin = a; }

	std::string serialize() const noexcept;
};

std::string proto_to_string( proto_version pv ) noexcept;

}

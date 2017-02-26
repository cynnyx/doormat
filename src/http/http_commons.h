#pragma once

#include <assert.h>

#include "../utils/utils.h"

namespace http
{

enum class proto_version : uint8_t
{
	UNSET,
	HTTP11,
	HTTP10,
	HTTP20,
};

enum class tranfer_encoding: uint8_t
{
	chunked,
	compress,
	deflate,
	gzip,
	identity
};

enum class content_type_t : uint16_t
{
	charset_utf8,
	text_html,
	text_css,
	text_plain,
	image_png,
	image_jpeg,
	image_x_icon,
	application_x_font_woff,
	application_x_font_ttf,
	application_json
};

// protocol syntactic elements
static constexpr auto space = " ";
static constexpr auto comma = ",";
static constexpr auto colon = ":";
static constexpr auto questionmark = "?";
static constexpr auto hash = "#";
static constexpr auto comma_space = ", ";
static constexpr auto semicolon_space = "; ";
static constexpr auto colon_space = ": ";
static constexpr auto slash = "/";
static constexpr auto cr = "\r";
static constexpr auto lf = "\n";
static constexpr auto crlf = "\r\n";
static constexpr auto transfer_chunked_termination = "0\r\n";

// protocol versions
static constexpr auto http10 = "HTTP/1.0";
static constexpr auto http11 = "HTTP/1.1";
static constexpr auto http20 = "HTTP/2.0";

// header fields
static constexpr auto hf_connection = "connection";
static constexpr auto hf_host = "host";
static constexpr auto hf_date = "date";
static constexpr auto hf_cyndate = "x-cyn-date";
static constexpr auto hf_content_len = "content-length";
static constexpr auto hf_accept_encoding = "accept-encoding";
static constexpr auto hf_content_encoding = "content-encoding";
static constexpr auto hf_transfer_encoding = "transfer-encoding";
static constexpr auto hf_allow = "allow";
static constexpr auto hf_via = "via";
static constexpr auto hf_server = "server";
static constexpr auto hf_maxforward = "max-forwards";
static constexpr auto hf_cyn_dest = "cyn-destination";
static constexpr auto hf_cyn_dest_port = "cyn-destination-port";
static constexpr auto hf_content_type = "content-type";

// header value
static constexpr auto hv_keepalive = "keep-alive";
static constexpr auto hv_connection_close = "close";
static constexpr auto hv_chunked = "Chunked";
static constexpr auto hv_gzip = "gzip";
static constexpr auto hv_charset_utf8 = "charset=UTF-8";
static constexpr auto hv_text_html = "text/html";
static constexpr auto hv_text_css = "text/css";
static constexpr auto hv_text_plain = "text/plain";
static constexpr auto hv_image_png = "image/png";
static constexpr auto hv_image_jpeg = "image/jpeg";
static constexpr auto hv_image_x_icon = "image/x-icon";
static constexpr auto hv_application_x_font_woff = "application/x-font-woff";
static constexpr auto hv_application_x_font_ttf = "application/x-font-ttf";
static constexpr auto hv_application_json = "application/json";

inline
const char* content_type(content_type_t type)
{
	switch(type)
	{
		case content_type_t::charset_utf8:
			return http::hv_charset_utf8;
		case content_type_t::text_html:
			return http::hv_text_html;
		case content_type_t::text_css:
			return http::hv_text_css;
		case content_type_t::text_plain:
			return http::hv_text_plain;
		case content_type_t::image_png:
			return http::hv_image_png;
		case content_type_t::image_jpeg:
			return http::hv_image_jpeg;
		case content_type_t::image_x_icon:
			return http::hv_image_x_icon;
		case content_type_t::application_x_font_woff:
			return http::hv_application_x_font_woff;
		case content_type_t::application_x_font_ttf:
			return http::hv_application_x_font_ttf;
		case content_type_t::application_json:
			return http::hv_application_json;
		default:
			assert(0);
	}
	return {};
}

inline
content_type_t content_type( const std::string& filename )
{
	// TODO DRM-225 - Using an appropriate library to understand MIME TYPE1
	using utils::ends_with;

	if(ends_with(filename, ".png"))
		return content_type_t::image_png;
	else if(ends_with(filename, ".jpeg") || ends_with(filename, ".jpg"))
		return content_type_t::image_jpeg;
	else if(ends_with(filename, ".css"))
		return content_type_t::text_css;
	else if(ends_with(filename, ".ico"))
		return content_type_t::image_x_icon;
	else if(ends_with(filename, ".woff"))
		return content_type_t::application_x_font_woff;
	else if(ends_with(filename, ".ttf"))
		return content_type_t::application_x_font_ttf;
	else
		return content_type_t::text_html;
}

}//http namespace

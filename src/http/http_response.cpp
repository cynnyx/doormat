#include "http_response.h"

namespace http
{
	const char* get_default_message( uint16_t status ) noexcept
	{
		switch (status)
		{
			case 100: return "Continue";
			case 101: return "Switching Protocols";
			case 200: return "OK";
			case 201: return "Created";
			case 202: return "Accepted";
			case 203: return "Non-Authoritative Information";
			case 204: return "No Content";
			case 205: return "Reset Content";
			case 206: return "Partial Content";
			case 300: return "Multiple Choices";
			case 301: return "Moved Permanently";
			case 302: return "Found";
			case 303: return "See Other";
			case 304: return "Not Modified";
			case 305: return "Use Proxy";
			case 307: return "Temporary Redirect";
			case 400: return "Bad Request";
			case 401: return "Unauthorized";
			case 402: return "Payment Required";
			case 403: return "Forbidden";
			case 404: return "Not Found";
			case 405: return "Method Not Allowed";
			case 406: return "Not Acceptable";
			case 407: return "Proxy Authentication Required";
			case 408: return "Request Timeout";
			case 409: return "Conflict";
			case 410: return "Gone";
			case 411: return "Length Required";
			case 412: return "Precondition Failed";
			case 413: return "Request Entity Too Large";
			case 414: return "Request-URI Too Long";
			case 415: return "Unsupported Media Type";
			case 416: return "Requested Range Not Satisfiable";
			case 417: return "Expectation Failed";
			case 418: return "I'm a teapot";
			case 500: return "Internal Server Error";
			case 501: return "Not Implemented";
			case 502: return "Bad Gateway";
			case 503: return "Service Unavailable";
			case 504: return "Gateway Timeout";
			case 505: return "HTTP Version Not Supported";
		}
		return "Damn!"; // Default value: Some Microsoft clients behave badly if the reason string is empty
	}

	std::string http_response::serialize() const noexcept
	{
		dstring msg;
		msg.append(protocol())
			.append(http::space)
			.append(std::to_string(_status_code))
			.append(http::space)
			.append(status_message())
			.append(http::crlf)
			.append(http_structured_data::serialize());
		return msg;
	}

	void http_response::status(uint16_t code) noexcept
	{
		_status_code = code;
		_status_message = get_default_message(code);
	}

	void http_response::status(uint16_t code, const std::string& msg) noexcept
	{
		_status_code = code;
		_status_message = msg;
	}

	bool http_response::operator==(const http_response&res)
	{
		return http_structured_data::operator==(res) && _status_code == res._status_code && _status_message == res._status_message;
	}

	bool http_response::operator!=(const http_response&res)
	{
		return res._status_code != _status_code || _status_message == res._status_message || !http_structured_data::operator==(res);
	}
}

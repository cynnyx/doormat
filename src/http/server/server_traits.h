#pragma once

namespace http {

class request;
class response;
class http_request;
class server_connection;

struct server_traits {
	using request_t = http::request;
	using response_t = http::response;
	using incoming_t = http::http_request;
	using connection_t = http::server_connection;
};

}

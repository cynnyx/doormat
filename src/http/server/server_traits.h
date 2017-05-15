#pragma once

namespace http {

class request;
class response;
class http_request;
class server_connection;

struct server_traits {

	using connection_t = http::server_connection;
	using remote_t = http::request;
	using local_t = http::response;
};

}

#pragma once

namespace http {

class client_request;
class client_response;
class client_connection;

struct client_traits {

	using connection_t = http::client_connection;
	using remote_t = http::client_response;
	using local_t = http::client_request;
};

}

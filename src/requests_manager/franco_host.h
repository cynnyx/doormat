#pragma once

#include "../chain_of_responsibility/node_interface.h"

namespace nodes
{

class franco_host : public node_interface
{
	bool active{false};
	dstring body;

	void handle_get( const http::http_request& );
	void handle_options( const http::http_request& );
	void handle_post( const http::http_request& );

public:
	using node_interface::node_interface;

	void on_request_preamble(http::http_request&& preamble);
	void on_request_body(dstring&& chunk);
	void on_request_trailer(dstring&& k, dstring&& v);
	void on_request_finished();
};

}

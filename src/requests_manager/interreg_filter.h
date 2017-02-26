#pragma once

#include "../chain_of_responsibility/node_interface.h"

namespace nodes
{

class interreg_filter : public node_interface
{
	bool allowed{true};
public:
	using node_interface::node_interface;
	void on_request_preamble(http::http_request&& preamble);
	void on_request_body(dstring&& chunk);
	void on_request_trailer(dstring&& k, dstring&& v);
	void on_request_finished();
};

}


#ifndef CONFIGURABLE_HEADER_FILTER_H
#define CONFIGURABLE_HEADER_FILTER_H

#include "../chain_of_responsibility/node_interface.h"

namespace nodes
{

class configurable_header_filter : public node_interface
{
	std::list<dstring> storage;

public:
	using node_interface::node_interface;

	void on_request_preamble(http::http_request&& preamble);
};

}
#endif // CONFIGURABLE_HEADER_FILTER_H

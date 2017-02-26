#ifndef DOORMAT_SIZE_FILTER_H
#define DOORMAT_SIZE_FILTER_H

#include "../chain_of_responsibility/node_interface.h"
#include "../errors/error_factory_async.h"

namespace nodes
{

class size_filter : public node_interface
{
	bool allowed{true};
	size_t limit{0};
	size_t total_message_size{0};
public:
	using node_interface::node_interface;
	void on_request_preamble(http::http_request&& preamble);
	void on_request_body(dstring&& chunk);
	void on_request_trailer(dstring&& k, dstring&& v);
	void on_request_finished();
};

}

#endif //DOORMAT_SIZE_FILTER_H

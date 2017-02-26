#ifndef DOOR_MAT_SYS_FILTER_H
#define DOOR_MAT_SYS_FILTER_H

#include "../chain_of_responsibility/node_interface.h"
#include "../errors/error_codes.h"

namespace nodes
{

class sys_filter : public node_interface
{
	errors::internal_error error_code;
public:
	using node_interface::node_interface;
	void on_request_preamble(http::http_request&& preamble);
	void on_request_body(dstring&& chunk);
	void on_request_trailer(dstring&& k, dstring&& v);
	void on_request_finished();
};

}
#endif //DOOR_MAT_SYS_FILTER_H

#ifndef ID_HEADER_ADDER_H
#define ID_HEADER_ADDER_H

#include "../chain_of_responsibility/node_interface.h"

namespace nodes
{

class id_header_adder : public node_interface
{
	void via_header( http::http_structured_data& );
	void server_header( http::http_structured_data& );
public:
	using node_interface::node_interface;

	void on_request_preamble(http::http_request&&);
	void on_header(http::http_response &&h);
};

}
#endif // ID_HEADER_ADDER_H

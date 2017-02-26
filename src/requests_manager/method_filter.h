#ifndef DOOR_MAT_METHOD_FILTER_H
#define DOOR_MAT_METHOD_FILTER_H

#include "../chain_of_responsibility/node_interface.h"
#include "../errors/error_codes.h"

namespace nodes
{

/**
 * @brief The method_filter class filters requests returning a
 * method_not_allowed page if the method is not supported.
 */
class method_filter : public node_interface
{
	bool allowed{true};
	bool is_allowed(http_method);
public:
	using node_interface::node_interface;
	void on_request_preamble(http::http_request&&);
	void on_request_body(dstring&&);
	void on_request_trailer(dstring&&,dstring&&);
	void on_request_finished();
};

} // namespace nodes


#endif //DOOR_MAT_METHOD_FILTER_H

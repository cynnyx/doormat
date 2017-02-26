#ifndef DOORMAT_REQUEST_STATS_H
#define DOORMAT_REQUEST_STATS_H

#include "../chain_of_responsibility/node_interface.h"

#include <chrono>

namespace nodes
{

class request_stats : public node_interface
{
	std::chrono::time_point<std::chrono::high_resolution_clock> start;

	void create_duration() const;
public:
	using node_interface::node_interface;
	void on_request_preamble( http::http_request&& preamble );
	void on_end_of_message();
	void on_error( const errors::error_code& ec );
};

}
#endif //DOORMAT_REQUEST_STATS_H

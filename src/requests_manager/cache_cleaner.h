#ifndef DOORMAT_CACHE_CLEANER_H
#define DOORMAT_CACHE_CLEANER_H

#include "../chain_of_responsibility/node_interface.h"

namespace nodes {
/** \brief this class responds to the need of evicting cache on demand by means of very specific HTTP Requests. */
class cache_cleaner : public node_interface
{
public:
	using node_interface::node_interface;

	void on_request_preamble(http::http_request &&req);
	void on_request_finished();
private:
	void generate_successful_response();
	bool matched{false};
	size_t freed{0};
};

}


#endif //DOORMAT_CACHE_CLEANER_H

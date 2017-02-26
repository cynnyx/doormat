#ifndef DOORMAT_CACHE_REQUEST_NORMALIZER_H
#define DOORMAT_CACHE_REQUEST_NORMALIZER_H

#include <boost/regex.hpp>

#include "../../chain_of_responsibility/node_interface.h"
#include "../../configuration/cache_normalization_rule.h"
#include "../../configuration/configuration_wrapper.h"
#include "../../service_locator/service_locator.h"

namespace nodes
{
class cache_request_normalizer : public node_interface
{
public:
	using node_interface::node_interface;
	void on_request_preamble(http::http_request &&req)
	{
		/** Apply some filter depending on the data that we have. */
		auto &rules = service::locator::configuration().get_cache_normalization_rules();

		req.remove_header("x-custom-cache-key");
		for(auto &&rule: rules)
			if(rule.matches(req)) { req.header("x-custom-cache-key", rule.get_cache_key(req).c_str()); break; }

		return node_interface::on_request_preamble(std::move(req));
	}

};

}



#endif //DOORMAT_CACHE_REQUEST_NORMALIZER_H

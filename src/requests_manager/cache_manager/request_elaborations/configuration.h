#ifndef DOORMAT_CONFIGURATION_H
#define DOORMAT_CONFIGURATION_H

#include <boost/regex.hpp>
#include "default.h"
#include "../../../service_locator/service_locator.h"
#include "../../../configuration/configuration_wrapper.h"

namespace cache_req_elaborations
{
	struct configuration : public default_elaboration
	{
		static bool should_cache(const http::http_request &req)
		{

			auto &cache_config = service::locator::configuration().get_cache_domains_config();
			for(auto &v: cache_config)
		    {
			   boost::smatch match;
			   boost::regex current_regex(v.second);
			   if(boost::regex_search(std::string(req.hostname()), match, current_regex))
			   {
				   return v.first;
			   }
		    }
			return true; //by default, we cache.
		}


		static std::string get_cache_key(const http::http_request &req, std::string prev)
		{
			auto &rules = service::locator::configuration().get_cache_normalization_rules();

			for(auto &&rule: rules)
				if(rule.matches(req)) return rule.get_cache_key(req);
			return prev;
		}
	};
}


#endif //DOORMAT_CONFIGURATION_H

#ifndef DOORMAT_CACHE_NORMALIZATION_RULE_H
#define DOORMAT_CACHE_NORMALIZATION_RULE_H

#include <boost/regex.hpp>
#include "../http/http_request.h"


struct cache_normalization_rule
{
	cache_normalization_rule(std::string vhost_matcher, std::string path_matcher, 
		std::string user_agent_matcher, std::string cache_key)
			: vhost_matcher{vhost_matcher}, path_matcher{path_matcher}, 
				user_agent_matcher{user_agent_matcher}, cache_key{cache_key}
	{
		parse();
	}

	void parse();
	bool matches(const http::http_request &req) const;

	std::string get_cache_key(const http::http_request &req) const
	{
		return cache_key_getter(req);
	}

	const boost::regex vhost_matcher;
	const boost::regex path_matcher;
	const boost::regex user_agent_matcher;
	const std::string cache_key;
	static const boost::regex cache_key_variable_matcher;

	std::function<std::string(const http::http_request&)> cache_key_getter;

};



#endif //DOORMAT_CACHE_NORMALIZATION_RULE_H

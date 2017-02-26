#include "cache_normalization_rule.h"

const boost::regex cache_normalization_rule::cache_key_variable_matcher{"<[:\\w\\-]+>"};

void cache_normalization_rule::parse()
{
	boost::smatch all_variable_matches;
	auto base_cache_key = cache_key;
	cache_key_getter = [base_cache_key](const http::http_request &req)
	{
		return base_cache_key;
	};
	boost::sregex_iterator begin(cache_key.begin(), cache_key.end(), cache_key_variable_matcher);
	boost::sregex_iterator end;
	for(; begin != end; ++begin) //iterate over all variables.
	{
		auto old_key_getter = cache_key_getter;
		std::string variable = begin->str();
		if(variable.find("<header:") == 0) //parsing an header.
		{
			/** introduce specific logic to replace header in this one*/
			cache_key_getter = [old_key_getter, variable](const http::http_request &req)
			{
				std::string output = old_key_getter(req);
				std::string value;
				dstring header_searched{variable.substr(8, variable.size()-9).data(), variable.size()-9};
				if( req.has(header_searched) )
					value = std::string(req.header(header_searched));

				size_t beginning = output.find(variable);
				while(beginning != std::string::npos)
				{
					output = output.replace(beginning, variable.size(), value);
					beginning = output.find(variable);
				}
				return output;
			};
			continue;
		}
		if(variable == "<default>")
		{
			cache_key_getter = [old_key_getter, variable](const http::http_request &req)
			{
				std::string output = old_key_getter(req);
				size_t beginning = output.find(variable);
				std::string value = std::string(req.hostname())+"$"+std::string(req.path())+"$"+std::string(req.query());
				while(beginning != std::string::npos)
				{
					output = output.replace(beginning, variable.size(), value);
					beginning = output.find(variable);
				}
				return output;
			};
			continue;
		}

		std::string error_msg = "variable " + variable + " is not allowed by cache_normalization_rule parser";
		throw std::logic_error{error_msg};
	}
}

bool cache_normalization_rule::matches(const http::http_request &req) const
{
	std::string user_agent_transformed = (req.has("user-agent")) ? req.header("user-agent") : "";

	return boost::regex_search(std::string{req.hostname().cdata(), req.hostname().size()}, vhost_matcher) &&
		boost::regex_search(std::string{req.path().cdata(), req.path().size()}, path_matcher) &&
		boost::regex_search(user_agent_transformed, user_agent_matcher);
}

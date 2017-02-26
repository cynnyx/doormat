#ifndef DOORMAT_HTTP_CACHING_H
#define DOORMAT_HTTP_CACHING_H

#include "default.h"
namespace cache_req_elaborations
{
	struct http_caching : public default_elaboration
	{
		static bool should_cache(const http::http_request &req)
		{
			if(req.method_code() != HTTP_GET) return false;
			if(req.has("pragma"))
			{
				std::string pragmaheader = req.header("pragma");
				if(boost::regex_search(pragmaheader, nocache))
				{
					return false;
				}
			}
			if(req.has("cache-control"))
			{
				std::string cache_control_header = req.header("cache-control");
				if(boost::regex_search(cache_control_header, nocache))
				{
					return false;
				}
			}
			return true;
		}


		static bool is_stale(const http::http_request &preamble, uint32_t age)
		{
			if (preamble.has("cache-control"))
			{
				boost::smatch max_age_match;
				std::string cache_control_header = preamble.header("cache-control");

				if (boost::regex_search(cache_control_header, max_age_match, maxage))
				{
					uint32_t max_age_requested = std::stoi(max_age_match[1]);
					if (max_age_requested <= age || max_age_requested == 0)
					{
						return true;
					}
				}

			}
			return false;
		}


		static uint32_t get_response_max_age(const http::http_response &res, uint32_t age)
		{
			bool has_max_age = false;
			uint32_t ttl = age;
			if (res.has("cache-control"))
			{
				boost::smatch max_age_match;
				std::string ccheader = std::string(res.header("cache-control"));
				if (boost::regex_search(ccheader, max_age_match, maxage))
				{
					/** Found a match with max-age; if max-age=0 then we cannot store the data in cache*/
					if (max_age_match.size() >= 2)
					{
						has_max_age = true;
						ttl = std::stoi(max_age_match[1]);
						if (res.has("age"))
						{
							ttl -= std::stoi(res.header("age"));
						}
					}
				}

			}
			if (res.has("expires") && !has_max_age) //max age has an higher priority; we do not use ttl == 0 to decide because if we subtract age this could happen.
			{
				std::tm tm = {};
				strptime(res.header("expires").cdata(), "%a %b %d %Y %H:%M:%S", &tm);
				auto tp = std::chrono::system_clock::from_time_t(std::mktime(&tm));
				ttl = std::chrono::duration_cast<std::chrono::seconds>(tp - std::chrono::system_clock::now()).count();
			}

			return ttl;
		}

	};
}

#endif //DOORMAT_HTTP_CACHING_H

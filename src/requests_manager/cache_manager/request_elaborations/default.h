#ifndef DOORMAT_DEFAULT_H
#define DOORMAT_DEFAULT_H


#include "../../../http/http_request.h"
#include "utils.h"

namespace cache_req_elaborations
{
struct default_elaboration
{
    static constexpr bool should_cache(const http::http_request &req)
    {
       return true;
    }


	static std::string get_cache_key(const http::http_request &req, std::string str)
	{
		return str;
	}

	static bool is_stale(const http::http_request &preamble, uint32_t age)
	{
		return false;
	}

	static constexpr uint32_t get_response_max_age(const http::http_response &res, uint32_t age)
	{
		return age;
	}

	static std::string refine_cache_key(const http::http_response &res, std::string prev)
	{
		return prev;
	}



};
}

#endif //DOORMAT_DEFAULT_H

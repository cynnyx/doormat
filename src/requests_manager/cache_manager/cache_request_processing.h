#ifndef DOORMAT_CACHE_AGGREGATE_FILTER_H
#define DOORMAT_CACHE_AGGREGATE_FILTER_H

#include "../../http/http_request.h"
#include <string>

template<typename ...Args>
struct cache_request_processing{};

template<typename F1, typename... OtherFilters>
struct cache_request_processing<F1, OtherFilters...>
{
	static constexpr bool should_cache(const http::http_request &req)
	{
		return F1::should_cache(req) && cache_request_processing<OtherFilters...>::should_cache(req);
	}

	static std::string get_cache_key(const http::http_request &req, std::string previously_elaborated_cache_key="")
	{
		return F1::get_cache_key(req, cache_request_processing<OtherFilters...>::get_cache_key(req, previously_elaborated_cache_key));
	}

	static constexpr bool is_stale(const http::http_request &req, uint32_t age)
	{
		return F1::is_stale(req, age) || cache_request_processing<OtherFilters...>::is_stale(req, age);
	}

	static constexpr uint32_t get_response_max_age(const http::http_response &res, uint32_t age)
	{
		return F1::get_response_max_age(res, cache_request_processing<OtherFilters...>::get_response_max_age(res, age));
	}

	static  std::string refine_cache_key(const http::http_response &res, std::string prev)
	{
		return F1::refine_cache_key(res, cache_request_processing<OtherFilters...>::refine_cache_key(res, prev));
	}
};


template<>
struct cache_request_processing<>
{
	static constexpr bool should_cache(const http::http_request &req){ return true; }

	static std::string get_cache_key(const http::http_request &req, std::string previously_elaborated_cache_key)
	{
		return "$"+std::string(req.hostname())+"$"+std::string(req.path())+"$"+std::string(req.query())+"$";;
	}

	static constexpr bool is_stale(const http::http_request &req, uint32_t age)
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




#endif
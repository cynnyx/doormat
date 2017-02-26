#ifndef DOORMAT_CACHE_MANAGER_H
#define DOORMAT_CACHE_MANAGER_H

#include "../../chain_of_responsibility/node_interface.h"
#include "../../cache/cache.h"
#include "../../cache/policies/fifo.h"
#include "../../http/http_codec.h"
#include <boost/regex.hpp>
#include "cache_request_processing.h"
#include "request_elaborations/configuration.h"
#include "request_elaborations/http_caching.h"

namespace nodes
{

class cache_manager : public node_interface
{
public:
	using node_interface::node_interface;

	void on_request_preamble(http::http_request&& preamble);
	void on_request_finished();

	void on_header(http::http_response &&response);
	void on_body(dstring&& chunk);
	void on_trailer(dstring&& k, dstring&& v);
	void on_end_of_message();
	void on_error(const errors::error_code &ec);// { if(!is_request_acked) on_request_ack(); error(ec); }

private:
	bool check_not_modified(const http::http_request &req)  const noexcept;
	bool is_in_cache = false;
	bool putting = false;
	bool request_finished = false;
	bool data_retrieved = false;
	bool not_modified = false;
	bool response_finished = false;
	bool is_cacheable = false;

	std::string key;
	std::vector<uint8_t> data{};
	uint32_t ttl{0};
	http::http_codec decoder, encoder;
	http::http_response cache_response;
	http_method mc{http_method::END};
	/** parses, adapts and sends back the contents retrieved from the cache.
	* */
	void manage_retrieved_content();

	/** generates a 304 not modified message as a reply to a client that specified an Etag
	*
	* */
	void generate304();
	/** the global cache used by all nodes. */
	cache<fifo_policy> *global_cache;
	/** Verifies if a response is eligible for cache.
	* \param response the response headers received by the origin server
	* \return the ttl (seconds) if the response can be cached; 0 otherwise
	* */
	static uint32_t response_filter(const http::http_response &response);
	/** Verifies if the resource required should be cached according to the domain/subdomain configuration
	*  \param URI the URI of the request
	*  \return true if the request can be cached; false otherwise.
	* */
	static bool domain_filter(const std::string &URI);

	static std::string calculate_tag(const http::http_response &response);

	static boost::regex nocache;
	static boost::regex maxage;
};

}

#endif //DOORMAT_CACHE_MANAGER_H

#include "cache_manager.h"
#include "../../configuration/configuration_wrapper.h"
#include "request_elaborations/gzip.h"
#include "../../stats/stats_manager.h"

namespace nodes
{
	//cache<fifo_policy> cache_manager::global_cache(UINT32_MAX, service::locator::configuration().cache_path()); //static cache used by everybody.
	boost::regex cache_manager::nocache{"no-cache|no-store"}; //
	boost::regex cache_manager::maxage{"[s-]?max[-]?age\\s?=\\s?(\\d+)"}; //

	using cache_request_processor = cache_request_processing<cache_req_elaborations::gzip, cache_req_elaborations::http_caching, cache_req_elaborations::configuration>;

	void cache_manager::on_request_preamble(http::http_request &&preamble)
	{

		if (!service::locator::configuration().cache_enabled())
		{
			return node_interface::on_request_preamble(std::move(preamble));
		}
		service::locator::stats_manager().enqueue(stats::stat_type::cache_requests_arrived, 1);
		if (!cache_request_processor::should_cache(preamble))
		{
			mc = preamble.method_code();
			if ((mc == http_method::HTTP_POST || mc == http_method::HTTP_PUT ||
				  mc == http_method::HTTP_DELETE || mc == http_method::HTTP_PATCH))
			{
				global_cache = cache<fifo_policy>::get_instance(UINT32_MAX, service::locator::configuration().cache_path()).get();
				key = cache_request_processor::get_cache_key(preamble);
				global_cache->invalidate(key);
			}
			return node_interface::on_request_preamble(std::move(preamble));
		}
		mc = preamble.method_code();
		is_cacheable = true;
		global_cache = cache<fifo_policy>::get_instance(UINT32_MAX, service::locator::configuration().cache_path()).get();
		key = cache_request_processor::get_cache_key(preamble);
		LOGTRACE("[Cache] request key is ", key);
		is_in_cache = global_cache->has(key);
		if (!is_in_cache) return node_interface::on_request_preamble(std::move(preamble));

		not_modified = check_not_modified(preamble);
		if (not_modified) return;

		/** Verification on the max requested age of the content.*/
		auto age = global_cache->get_age(key);
		is_in_cache = !cache_request_processor::is_stale(preamble, age);
		if(!is_in_cache) return node_interface::on_request_preamble(std::move(preamble));

		//take a shortcut; start to ask for data awaiting for "on request finished" event.
		global_cache->get(key, [this](std::vector<uint8_t> d)
		{
			LOGTRACE("[Cache] data has been retrieved!");
			data = std::move(d);
			data_retrieved = true;
			manage_retrieved_content();
		});

		return;
	}


	void cache_manager::on_request_finished()
	{
		//request was finished; hence we get back with data!
		if (not_modified)
		{
			generate304();
			return;
		}

		if (is_in_cache)
		{
			request_finished = true;
			manage_retrieved_content();
			return;
		}
		node_interface::on_request_finished();
	}

	bool cache_manager::check_not_modified(const http::http_request &req) const noexcept
	{
		if (req.has("if-none-match")) //conditional query; verify tag to respond with 304; otherwise retrieve from cache.
		{
			LOGTRACE("[Cache] if none match", key);
			/** Need for a precise match to return back a 304*/
			std::string strict_matcher = req.header("if-none-match");
			if (global_cache->verify_version(key, strict_matcher))
			{
				//not_modified = true;
				return true;
			} //otherwise we take the version from the cache; we don't care if the content cached here is stale.
		}
		if (req.has("if-modified-since")) /** Header is in human format; just proceed to parse it and understand the difference. */
		{
			LOGTRACE("[Cache] if modified since", key);
			std::tm tm = {};
			strptime(req.header("if-modified-since").cdata(), "%a %b %d %Y %H:%M:%S", &tm);
			auto tp = std::chrono::system_clock::from_time_t(std::mktime(&tm));
			/** The latest modification date that we know of is: */
			if (tp >= std::chrono::system_clock::now() - std::chrono::seconds(global_cache->get_age(key)))
			{
				//not_modified = true;
				return true;
			}
		}
		return false;
	}


	void cache_manager::on_header(http::http_response &&response)
	{
		assert(!is_in_cache);

		LOGDEBUG("status: cacheable", (mc == HTTP_GET), " response allows: ", response_filter(response));
		if (is_cacheable && response_filter(response))
		{
			LOGTRACE("storing the element");
			ttl = cache_request_processor::get_response_max_age(response, 3600);
			std::string tag = calculate_tag(response);
			std::string etag = (response.has("etag")) ? response.header("etag") : "";
			std::chrono::system_clock::time_point creation_time = (response.has("age")) ?
																  std::chrono::system_clock::now() -
																  std::chrono::seconds(std::stoi(
																		  std::string(response.header("age"))))
																						: std::chrono::system_clock::now();
			//refines the cache key depending on the status of the response.
			key = cache_request_processor::refine_cache_key(response, key);
			putting = global_cache->begin_put(key, ttl, creation_time, tag, etag);
			if (putting)
				global_cache->put(key, encoder.encode_header(response));

			//store it on cache.
		}
		response.header("x-cache-status", "MISS");
		node_interface::on_header(std::move(response));
	}


	void cache_manager::on_body(dstring &&body)
	{
		assert(!is_in_cache);
		if (putting)
			global_cache->put(key, encoder.encode_body(body));
		return node_interface::on_body(std::move(body));
	}


	void cache_manager::on_trailer(dstring &&k, dstring &&v)
	{
		assert(!is_in_cache);
		if (putting)
			global_cache->put(key, encoder.encode_trailer(k, v));
		return node_interface::on_trailer(std::move(k), std::move(v));
	}


	void cache_manager::on_end_of_message()
	{
		assert(!is_in_cache);
		if (putting)
		{
			global_cache->put(key, encoder.encode_eom());
			global_cache->end_put(key, true);
		} //commit.
		node_interface::on_end_of_message();
	}

	void cache_manager::on_error(const errors::error_code &ec)
	{
		assert(!is_in_cache);
		if (putting) global_cache->end_put(key, false);
		node_interface::on_error(ec);
	}

	/** fixme: with this process we scan for information twice, once to check for validity and the other time
	 * to extract informations.*/
	uint32_t cache_manager::response_filter(const http::http_response &response)
	{
		if (response.has("authorization")) return false;
		auto code = response.status_code();
		LOGTRACE("[cache] response status code is ", code);
		if (code != 200 && code != 203 && code != 300 && code != 301 && code != 410) return false;
		//HTTP 1.0 backwards compatibility

		if (response.has("pragma"))
		{
			auto pragma_value = std::string(response.header("pragma")); //.find("no-cache");
			std::transform(pragma_value.begin(), pragma_value.end(), pragma_value.begin(), ::tolower);
			if (pragma_value.find("no-cache")) return false;
		}

		if (response.has("cache-control"))
		{
			std::string cache_control_header = std::string(response.header("cache-control"));
			/*** now parse everything*/
			if (boost::regex_search(cache_control_header, nocache))
			{
				/** No cache: this means that we cannot store the content anywhere. */
				return false;
			}
			boost::smatch max_age_match;
			LOGTRACE("[cache] cache control header is.. ", cache_control_header);
			if (boost::regex_search(cache_control_header, max_age_match, maxage))
			{
				/** Found a match with max-age; if max-age=0 then we cannot store the data in cache*/
				if (max_age_match.size() < 2)
				{
					LOGTRACE("[cache] misformed maxage directive with count", max_age_match.size());
					return false; //malformed directive
				}
				LOGTRACE("[cache] response filter allows caching");
				return true;
			}
		}
		/** max-age, if present, has an higher priority than this one. */
		if (response.has("expires"))
		{
			std::string expiry_date = std::string(response.header("expires"));
			/** Try to extract the expiration date; if it is greather than ours than it is ok.*/
			std::tm tm = {};
			strptime(expiry_date.data(), "%a %b %d %Y %H:%M:%S", &tm);
			auto tp = std::chrono::system_clock::from_time_t(std::mktime(&tm));
			return tp > std::chrono::system_clock::now();
		}

		return true;
	}


	std::string cache_manager::calculate_tag(const http::http_response &response)
	{
		if (!response.has("content-type")) return {};
		auto content_type = response.header("content-type");
		if (content_type.find("html") != std::string::npos) return "html";
		if (content_type.find("javascript") != std::string::npos) return "js";
		if (content_type.find("css") != std::string::npos) return "css";
		return {};
	}


	void cache_manager::generate304()
	{
		http::http_response response;
		response.protocol(http::proto_version::HTTP11);
		response.status(304);

		node_interface::on_header(std::move(response));
		node_interface::on_end_of_message();
	}


	void cache_manager::manage_retrieved_content()
	{
		if (request_finished && data_retrieved)
		{
			auto decoding_start_cb = [this](http::http_structured_data **data)
			{ //we will need to assign to data the element that we want filled.
				*data = &cache_response;
			};

			auto headers_readed_cb = [this]()
			{ //all headers have been decoded; add a cache management one and go on
				cache_response.header("x-cache-status", "HIT");
				service::locator::stats_manager().enqueue(stats::stat_type::cache_hits, 1);
				node_interface::on_header(std::move(cache_response));
			};

			auto body_received_cb = [this](dstring &&d)
			{
				assert(!response_finished);
				node_interface::on_body(std::move(d));
			};

			auto trailer_received_callback = [this](dstring &&k, dstring &&v)
			{
				assert(!response_finished);
				node_interface::on_trailer(std::move(k), std::move(v));
			};

			auto codec_ccb = [this]()
			{
				assert(!response_finished);
				response_finished = true;
				node_interface::on_end_of_message();
			};

			auto codec_fcb = [this](int err, bool &fatal)
			{
				assert(false); //this should not happen.
			};

			decoder.register_callback(std::move(decoding_start_cb), std::move(headers_readed_cb),
				std::move(body_received_cb), std::move(trailer_received_callback),
				std::move(codec_ccb), std::move(codec_fcb));
			decoder.decode(reinterpret_cast<char*>(data.data()), data.size());
		}
	}

}

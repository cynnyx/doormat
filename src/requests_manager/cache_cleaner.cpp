#include <boost/regex.hpp>
#include "cache_cleaner.h"
#include "../constants.h"
#include "../cache/cache.h"
#include "../configuration/configuration_wrapper.h"
#include "../cache/policies/fifo.h"

namespace nodes
{

	const static boost::regex rgx_simple_clear{"/cache/clear"};
	const static boost::regex rgx_clear_tag{"/cache/tag/([[:word:]]+)/clear"};
	void cache_cleaner::on_request_preamble(http::http_request &&req)
	{
		if(req.method_code() != http_method::HTTP_POST) return base::on_request_preamble(std::move(req));
		if(!utils::i_starts_with(req.hostname(), sys_subdomain_start())) return base::on_request_preamble(std::move(req));
		auto &path = req.path();
		boost::cmatch simple, tag;
		if(boost::regex_match(path.cbegin(), path.cend(), simple, rgx_simple_clear))
		{
			matched = true;
			auto cache_ptr = cache<fifo_policy>::get_instance(UINT32_MAX, service::locator::configuration().cache_path()).get();
			freed = cache_ptr->clear_all();
			return;
		}
		if(boost::regex_match(path.cbegin(), path.cend(), tag, rgx_clear_tag))
		{
			matched = true;
			auto cache_ptr = cache<fifo_policy>::get_instance(UINT32_MAX, service::locator::configuration().cache_path()).get();
			freed = cache_ptr->clear_tag(tag[1]);
			return;
		}

		return base::on_request_preamble(std::move(req));
	}

	void cache_cleaner::on_request_finished()
	{
		if(matched) return generate_successful_response();
		return base::on_request_finished();
	}

	void cache_cleaner::generate_successful_response()
	{
		std::string response_message = "Doormat just deleted " + std::to_string(freed) + " bytes from the cache";
		http::http_response res;
		res.status(200);
		res.content_len(response_message.size());
		base::on_header(std::move(res));
		dstring d;
		d.append(response_message);
		base::on_body(std::move(d));
		base::on_end_of_message();
	}
}
#ifndef DOORMAT_GZIP_H
#define DOORMAT_GZIP_H

#include "default.h"
#include "../../../configuration/configuration_wrapper.h"
#include "../../gzip_filter.h"

namespace cache_req_elaborations
{

	struct gzip : public default_elaboration
	{
		/**
		 *
		 *
		 * */
		static std::string get_cache_key(const http::http_request &req, std::string prev)
		{
			bool gzip_enabled = service::locator::configuration().get_compression_enabled() && req.has(http::hf_accept_encoding, [](const dstring& v){ return nodes::gzip_filter::parse_accepted_encoding(v);});
			if(gzip_enabled)  prev += "gzip$";
			return prev;
		}



		static std::string refine_cache_key(const http::http_response &res, std::string prev)
		{
			if (prev.find("gzip$") != std::string::npos && (!res.has(http::hf_content_encoding) ||
			                     res.header(http::hf_content_encoding) != http::hv_gzip))
			{
				//gzip was supposedly enabled, but on the response we received a content that was not zipped. hence we change the key!
				prev = prev.substr(0, prev.size() - 5);
			}
			return prev;
		}
	};
}

#endif //DOORMAT_GZIP_H

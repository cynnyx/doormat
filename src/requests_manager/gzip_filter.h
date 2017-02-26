#pragma once
#include "../chain_of_responsibility/node_interface.h"
#include "../log/access_record.h"
#include "../utils/dstring_factory.h"

#include <memory>
#include <queue>
#include <zlib.h>
#include <boost/optional.hpp>

namespace nodes
{

class gzip_filter : public node_interface
{
	struct accept_encoding_item
	{
		accept_encoding_item() : name(), priority(0) {}
		accept_encoding_item(const dstring& val)
		{
			//Trim leading spaces and commas
			auto b = std::find_if_not(val.cbegin(), val.cend(), [](char c){ return c == ' '|| c== ',';});
			auto e = std::find(b, val.cend(),';');
			name = val.substr(b, e,::tolower);

			auto q_len = std::distance(e+1,val.cend());
			if( q_len > 2 )
			{
				if( memcmp(e+1, "q=", 2) == 0 )
				{
					q_len -=2;
					if(q_len > 5) q_len=5;

					char q_str[6];
					memcpy(q_str, e + 3, q_len );
					q_str[q_len] = 0;
					priority = 1000 * atof(q_str);
				}
			}
		}

		dstring name;
		uint16_t priority{1000};
	};

	bool active{false};
	bool enabled{false};
	uint8_t level{0};
	uint32_t min_size{0};

	std::unique_ptr<z_stream_s> _stream;
	std::queue<dstring> _in_buf;
	dstring_factory _dsf;
	boost::optional<http::http_response> res;

	bool init_compressor();
	bool compress(int fm = Z_NO_FLUSH, std::function<void(dstring&)> handler = nullptr);

public:
	gzip_filter(req_preamble_fn request_preamble, req_chunk_fn request_body,
		req_trailer_fn request_trailer, req_canceled_fn request_canceled, req_finished_fn request_finished, header_callback hcb, body_callback bcb,
		trailer_callback tcb, end_of_message_callback eomcb, error_callback ecb,
		response_continue_callback rccb,logging::access_recorder *aclogger = nullptr);
	~gzip_filter();
	static bool parse_accepted_encoding(const dstring& val);


	void on_request_preamble(http::http_request&&);
	void on_request_trailer(dstring&&, dstring&&);

	void on_header(http::http_response&&);
	void on_body(dstring&&);
	void on_end_of_message();
};

}

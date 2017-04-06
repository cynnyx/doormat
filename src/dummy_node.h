#pragma once

#include <memory>
#include <boost/asio.hpp>
#include <boost/date_time.hpp>

#include "service_locator/service_locator.h"
#include "io_service_pool.h"
#include "chain_of_responsibility/node_interface.h"
#include "http/http_structured_data.h"
#include "utils/log_wrapper.h"
#include "log/format.h"

static constexpr auto message = "Ave client, dummy node says hello";
static constexpr auto host = "localhost";
static constexpr auto date = "Tue, 17 May 2016 14:53:09 GMT";

struct dummy_node : public node_interface
{
	using node_interface::node_interface;
	void on_request_preamble(http::http_request&& msg)
	{
		http_message_protocol_copy = std::move(msg);
		procrastinator.reset(new boost::asio::deadline_timer(service::locator::service_pool().get_thread_io_service()));
		procrastinator->expires_from_now(boost::posix_time::microseconds(1));
		procrastinator->async_wait([this](const boost::system::error_code& ec)
			{
				if(!ec)
				{
					LOGERROR("Fast answer!");
					http::http_response preamble;
					preamble.protocol(http_message_protocol_copy.protocol_version());
					//preamble.hostname(host);
					preamble.date(date);
					preamble.status(200);
					preamble.header(http::hf_content_type, http::hv_text_plain);
					preamble.content_len(strlen(message));
					on_header(std::move(preamble));
					on_body(message);
					on_end_of_message();
				}
				else
					LOGTRACE(ec.message());
			}
		);
	}

	void on_request_body(dstring&& chunk)
	{
		LOGTRACE("dummy node got a chunk");
	}
	
	void on_request_trailer(dstring&& k, dstring&& v) { }
	void on_request_canceled(const errors::error_code &err) { }
	void on_request_finished() { }

	~dummy_node()
	{
		LOGTRACE("dummy node is dead");
	}

private:
	std::unique_ptr<boost::asio::deadline_timer> procrastinator;
	http::http_request http_message_protocol_copy;
};

class test_node : public node_interface
{
public:
	using node_interface::node_interface;
	void on_request_preamble(http::http_request&& msg)
	{
		msg.ssl(false);
		msg.schema("http");
		msg.urihost("localhost");
		msg.port("2000");
		node_interface::on_request_preamble( std::move( msg ) );
	}
};



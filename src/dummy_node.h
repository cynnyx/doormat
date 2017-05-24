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

static const std::string message = "Ave client, dummy node says hello";
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
					preamble.content_len(message.size());
					on_header(std::move(preamble));
					auto ptr = std::make_unique<char[]>(message.size());
					std::copy(message.begin(), message.end(), ptr.get());
					on_body(std::move(ptr), message.size());
					on_end_of_message();
				}
				else
					LOGTRACE(ec.message());
			}
		);
	}

	void on_request_body(data_t data, size_t len)
	{
		LOGTRACE("dummy node got a chunk");
	}
	
	void on_request_trailer(std::string&& k, std::string&& v) { }
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
		msg.setParameter("hostname", "localhost");
		msg.setParameter("port", "2000"); 
		node_interface::on_request_preamble( std::move( msg ) );
	}
};

class tls_test_node : public node_interface
{
public:
	using node_interface::node_interface;
	void on_request_preamble(http::http_request&& msg)
	{
		msg.ssl(true);
		msg.schema("https");
		msg.urihost("localhost");
		msg.port("4000");
		msg.setParameter("hostname", "localhost");
		msg.setParameter("port", "4000"); 
		node_interface::on_request_preamble( std::move( msg ) );
	}
};




#include "nodes/common.h"
#include "mock_server/mock_server.h"
#include "../src/requests_manager/size_filter.h"
#include "../src/requests_manager/client_wrapper.h"

namespace
{
using namespace test_utils;
struct size_client_integration : public preset::test {};

static std::string fakebody(800,'Z');

TEST_F(size_client_integration, too_large_content_length)
{
	http::http_request req;
	req.protocol(http::proto_version::HTTP11);
	req.method("PUT");
	req.keepalive(false);
	req.content_len(4096);

	ch = make_unique_chain<node_interface, first_node, nodes::size_filter, nodes::client_wrapper>();
	ch->on_request_preamble(std::move(req));

	EXPECT_TRUE(first_node::err == INTERNAL_ERROR(errors::http_error_code::entity_too_large) )<<first_node::err;
}

TEST_F(size_client_integration, too_large_body)
{
	std::unique_ptr<mock_server> server;
	std::function<void(std::string)> read_fn;

	auto init_fn = [this, &server, read_fn](boost::asio::io_service& ios) mutable
	{
		preset::init_thread_local();
		ch = make_unique_chain<node_interface, first_node, nodes::size_filter, nodes::client_wrapper>();

		http::http_request req;
		req.protocol(http::proto_version::HTTP11);
		req.method("PUT");
		req.keepalive(false);
		req.content_len(999);

		read_fn = [this, &server, read_fn](std::string)
		{
			if(first_node::req_body == 1)
				ch->on_request_body(fakebody.c_str());
			server->read(1, read_fn);
			service::locator::socket_pool().stop();
			server->stop();
			service::locator::service_pool().allow_graceful_termination();
		};

		first_node::res_stop_fn = [this, &ios]()
		{
			EXPECT_EQ(first_node::res_eom, 0);
			EXPECT_TRUE(first_node::err == INTERNAL_ERROR(errors::http_error_code::entity_too_large) )<<first_node::err;
			ios.post([this](){ch.reset();});
		};

		server.reset(new mock_server);
		server->start([&server, read_fn](){server->read(500, read_fn);});

		ch->on_request_preamble(std::move(req));
		ch->on_request_body(std::move(fakebody.c_str()));
	};

	service::locator::service_pool().run(init_fn);
}

}

#include "../src/requests_manager/franco_host.h"
#include "common.h"

namespace
{
using namespace test_utils;
struct franco_host_test : public preset::test{};

dstring francos{"franco.host.com"};

}

TEST_F(franco_host_test, not_filtering)
{
	ch = make_unique_chain<node_interface, first_node, nodes::franco_host, last_node>();
	http::http_request req;
	req.method(HTTP_GET);
	ch->on_request_preamble(std::move(req));
	ch->on_request_body({});
	ch->on_request_trailer({},{});
	ch->on_request_finished();

	EXPECT_TRUE(last_node::request.is_initialized());
	EXPECT_EQ(last_node::req_body, 1);
	EXPECT_EQ(last_node::req_trailer, 1);
	EXPECT_EQ(last_node::req_eom, 1);
}


TEST_F(franco_host_test, unprivileged)
{
	http::http_request req;
	req.method(HTTP_GET);
	req.origin(boost::asio::ip::address::from_string("192.0.0.1"));
	req.hostname(francos);
	ch = make_unique_chain<node_interface, first_node, nodes::franco_host, blocked_node>();
	ch->on_request_preamble(std::move(req));
	ch->on_request_body({});
	ch->on_request_trailer({},{});
	ch->on_request_finished();

	EXPECT_TRUE(first_node::err == INTERNAL_ERROR(errors::http_error_code::forbidden))<<first_node::err;
}

TEST_F(franco_host_test, wrong_method)
{
	http::http_request req;
	req.method(HTTP_DELETE);
	req.origin(boost::asio::ip::address::from_string("127.0.0.1"));
	req.hostname(francos);
	ch = make_unique_chain<node_interface, first_node, nodes::franco_host, blocked_node>();
	ch->on_request_preamble(std::move(req));
	ch->on_request_body({});
	ch->on_request_trailer({},{});
	ch->on_request_finished();

	EXPECT_TRUE( first_node::err == INTERNAL_ERROR(errors::http_error_code::method_not_allowed) ) << first_node::err.code();

}

TEST_F(franco_host_test, privileged) // we expect filtering and stats in the response
{
	http::http_request req;
	req.method(HTTP_GET);
	req.hostname(francos);
	req.origin(boost::asio::ip::address::from_string("127.0.0.1"));

	ch = make_unique_chain<node_interface, first_node, nodes::franco_host, blocked_node>();
	ch->on_request_preamble(std::move(req));
	ch->on_request_body({});
	ch->on_request_trailer({},{});
	ch->on_request_finished();

	// check the filtering
	ASSERT_TRUE(first_node::response.is_initialized());
	EXPECT_EQ(first_node::response->status_code(), 200);
	EXPECT_EQ(first_node::response->content_len(), service::locator::stats_manager().serialize().size());

	auto appjsonptr = http::content_type( http::content_type_t::application_json );
	EXPECT_TRUE(first_node::response->has(http::hf_content_type, appjsonptr));
}

TEST_F(franco_host_test, options)
{
	http::http_request req;
	req.method(HTTP_OPTIONS);
	req.hostname(francos);
	req.origin(boost::asio::ip::address::from_string("127.0.0.1"));

	ch = make_unique_chain<node_interface, first_node, nodes::franco_host, blocked_node>();
	ch->on_request_preamble(std::move(req));
	ch->on_request_body({});
	ch->on_request_trailer({},{});
	ch->on_request_finished();

	// check the filtering
	ASSERT_TRUE(first_node::response.is_initialized());
	EXPECT_EQ(first_node::response->status_code(), 200);
	EXPECT_EQ(first_node::response->content_len(), 0);
	EXPECT_TRUE(first_node::response->has(http::hf_allow, "GET,OPTIONS"));
}

TEST_F(franco_host_test, post)
{
	http::http_request req;
	req.method(HTTP_POST);
	req.hostname(francos);
	req.origin(boost::asio::ip::address::from_string("127.0.0.1"));
	req.header(http::hf_cyn_dest,"127.0.0.1");
	req.header(http::hf_cyn_dest_port,"8454");

	ch = make_unique_chain<node_interface, first_node, nodes::franco_host, blocked_node>();
	ch->on_request_preamble(std::move(req));
	ch->on_request_body({});
	ch->on_request_trailer({},{});
	ch->on_request_finished();

	// check the filtering
	ASSERT_TRUE(first_node::response.is_initialized());
	EXPECT_EQ(first_node::response->status_code(), 200);
	EXPECT_EQ(first_node::response->content_len(), 0);

}

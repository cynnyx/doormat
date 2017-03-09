#include "../src/requests_manager/sys_filter.h"
#include "common.h"

namespace
{
using namespace test_utils;
struct sys_filter_test : public preset::test {};
}

TEST_F(sys_filter_test, not_filtering)
{
	http::http_request req;

	auto ch = make_unique_chain<node_interface, first_node, nodes::sys_filter, last_node>();
	ch->on_request_preamble(std::move(req));
	ch->on_request_body({});
	ch->on_request_trailer({},{});
	ch->on_request_finished();

	ASSERT_TRUE(last_node::request.is_initialized());
	EXPECT_EQ(last_node::req_body, 1U);
	EXPECT_EQ(last_node::req_trailer, 1U);
	EXPECT_EQ(last_node::req_eom, 1U);
}

TEST_F(sys_filter_test, unprivileged)
{
	http::http_request req;
	req.hostname("sys.test.com");

	auto ch = make_unique_chain<node_interface, first_node, nodes::sys_filter, blocked_node>();
	ch->on_request_preamble(std::move(req));
	ch->on_request_body({});
	ch->on_request_trailer({},{});
	ch->on_request_finished();

	EXPECT_TRUE(first_node::err == INTERNAL_ERROR(errors::http_error_code::forbidden))<<first_node::err;
}


TEST_F(sys_filter_test, privileged)
{
	http::http_request req;
	req.origin(boost::asio::ip::address::from_string("127.0.0.1"));
	req.hostname("sys.test.com");

	auto ch = make_unique_chain<node_interface, first_node, nodes::sys_filter, last_node>();
	ch->on_request_preamble(std::move(req));
	ch->on_request_body({});
	ch->on_request_trailer({},{});
	ch->on_request_finished();

	// check the filtering
	ASSERT_TRUE(last_node::request.is_initialized());
	EXPECT_EQ(last_node::req_body, 1U);
	EXPECT_EQ(last_node::req_trailer, 1U);
	EXPECT_EQ(last_node::req_eom, 1U);
}



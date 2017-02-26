#include "../src/requests_manager/interreg_filter.h"
#include "common.h"

namespace
{
static constexpr auto interreg = "interreg.test.com";
static constexpr auto unprotected = "test.com";

using namespace test_utils;
struct interreg_filter_test : public preset::test {};
}

TEST_F(interreg_filter_test, interreg_allowed)
{
	http::http_request req;
	req.hostname(interreg);
	req.origin(boost::asio::ip::address::from_string("127.0.0.1"));
	req.protocol(http::proto_version::HTTP11);

	auto ch = make_unique_chain<node_interface, first_node, nodes::interreg_filter, last_node>();
	ch->on_request_preamble(std::move(req));
	ch->on_request_body({});
	ch->on_request_trailer({}, {});
	ch->on_request_finished();

	EXPECT_TRUE(last_node::request.is_initialized());
	EXPECT_EQ(last_node::req_body,1);
	EXPECT_EQ(last_node::req_trailer,1);
	EXPECT_EQ(last_node::req_eom,1);
}

TEST_F(interreg_filter_test, interreg_blocked)
{
	http::http_request req;
	req.hostname(interreg);
	req.origin(boost::asio::ip::address::from_string("192.168.0.1"));
	req.protocol(http::proto_version::HTTP11);

	auto ch = make_unique_chain<node_interface, first_node, nodes::interreg_filter, blocked_node>();
	ch->on_request_preamble(std::move(req));
	ch->on_request_body({});
	ch->on_request_trailer({},{});
	ch->on_request_finished();

	EXPECT_TRUE( first_node::err == INTERNAL_ERROR(errors::http_error_code::forbidden) ) << first_node::err.code();

}

TEST_F(interreg_filter_test, unprotected_host)
{
	http::http_request req;
	req.hostname(unprotected);
	req.origin(boost::asio::ip::address::from_string("192.168.0.1"));
	req.protocol(http::proto_version::HTTP11);

	auto ch = make_unique_chain<node_interface, first_node, nodes::interreg_filter, last_node>();
	ch->on_request_preamble(std::move(req));
	ch->on_request_body({});
	ch->on_request_trailer({},{});
	ch->on_request_finished();

	ASSERT_TRUE(last_node::request.is_initialized());
	EXPECT_EQ(last_node::req_body,1);
	EXPECT_EQ(last_node::req_trailer,1);
	EXPECT_EQ(last_node::req_eom,1);
}

#include "../../src/requests_manager/size_filter.h"
#include "common.h"

using namespace test_utils;
struct size_filter_test : public preset::test{};

TEST_F(size_filter_test, request_well_formed)
{
	http::http_request req;
	req.content_len(999);

	ch = make_unique_chain<node_interface, first_node, nodes::size_filter, last_node>();
	ch->on_request_preamble(std::move(req));
	ch->on_request_finished();

	EXPECT_TRUE(first_node::response.is_initialized());
	EXPECT_FALSE(first_node::err);
}

TEST_F(size_filter_test, too_large_content_length)
{
	http::http_request req;
	req.content_len(5454545);

	ch = make_unique_chain<node_interface, first_node, nodes::size_filter, blocked_node>();
	ch->on_request_preamble(std::move(req));

	EXPECT_TRUE(first_node::err == INTERNAL_ERROR(errors::http_error_code::entity_too_large) )<<first_node::err;
}

TEST_F(size_filter_test, too_large_body_wrong_cl)
{
	http::http_request req;
	req.content_len(999);

	ch = make_unique_chain<node_interface, first_node, nodes::size_filter, last_node>();
	ch->on_request_preamble(std::move(req));

	std::string fakebody(999,'Z');
	ch->on_request_body(fakebody.c_str());
	ch->on_request_body(fakebody.c_str());

	EXPECT_TRUE(last_node::request.is_initialized());
	EXPECT_EQ(last_node::req_body, 1U);
	EXPECT_TRUE(first_node::err == INTERNAL_ERROR(errors::http_error_code::entity_too_large) )<<first_node::err;
}


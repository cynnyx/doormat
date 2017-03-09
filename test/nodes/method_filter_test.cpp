#include "../src/requests_manager/method_filter.h"
#include "common.h"

static constexpr auto host = "localhost";
static constexpr auto path = "/public";

using namespace test_utils;
struct method_filter_test : public preset::test {};

TEST_F(method_filter_test, all_allowed_methods)
{
	const char *allowed_methods[] = {"GET", "POST", "PUT", "DELETE", "OPTIONS", "HEAD", "PATCH"};
	ch = make_unique_chain<node_interface, first_node, nodes::method_filter, last_node>();
	for( auto&& m : allowed_methods )
	{
		http::http_request req;
		req.protocol(http::proto_version::HTTP11);
		req.urihost(host);
		req.path(path);
		req.method(m);
		ch->on_request_preamble(std::move(req));
		ch->on_request_body({});
		ch->on_request_trailer({}, {});
		ch->on_request_finished();
		EXPECT_TRUE(last_node::request.is_initialized());
		EXPECT_EQ(last_node::req_body, 1U);
		EXPECT_EQ(last_node::req_trailer, 1U);
		EXPECT_EQ(last_node::req_eom, 1U);
		last_node::reset();
	}
}

TEST_F(method_filter_test, all_unsupported_methods)
{
	const char *filtered_methods[] =
	{"BASELINE-CONTROL","BIND","CHECKIN","CHECKOUT","CONNECT","COPY", "LABEL","LINK",
	"LOCK","MERGE","MKACTIVITY","MKCALENDAR","MKCOL","MKREDIRECTREF","MKWORKSPACE","MOVE",
	"ORDERPATCH", "PRI", "PROPFIND", "PROPPATCH", "REBIND","REPORT","SEARCH","TRACE"
	"UNBIND","UNCHECKOUT","UNLINK","UNLOCK","UPDATE","UPDATEREDIRECTREF","VERSION-CONTROL"};

	ch = make_unique_chain<node_interface, first_node, nodes::method_filter, blocked_node>();
	for( auto&& m : filtered_methods )
	{
		http::http_request req;
		req.protocol(http::proto_version::HTTP11);
		req.urihost(host);
		req.path(path);
		req.method(m);
		ch->on_request_preamble(std::move(req));
		ch->on_request_body({});
		ch->on_request_trailer({},{});
		ch->on_request_finished();

		EXPECT_TRUE( first_node::err == INTERNAL_ERROR(errors::http_error_code::method_not_allowed) ) << first_node::err.code();

		first_node::reset();
	}
}


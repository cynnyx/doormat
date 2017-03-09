#include "../src/requests_manager/header_filter.h"
#include "common.h"

namespace
{
using namespace test_utils;
struct header_filter_test : public preset::test {};
}

TEST_F(header_filter_test, filter_headers)
{
	http::http_request req;
	req.hostname("hostname");
	req.header("connection", "");
	req.header("keep-alive", "");
	req.header("public","");
	req.header("proxy-authenticate","");
	req.header("transfer-encoding","chunked");
	req.header("upgrade","");
	req.header("GoodEnough...","");

	ch = make_unique_chain<node_interface, first_node, nodes::header_filter, last_node>();
	ch->on_request_preamble(std::move(req));
	ch->on_request_body({});
	ch->on_request_trailer({},{});
	ch->on_request_finished();

	// check the filtering
	EXPECT_TRUE(last_node::request.is_initialized());
	EXPECT_GT(last_node::request->hostname().size(), 0U);
	EXPECT_EQ(last_node::req_body, 1U);
	EXPECT_EQ(last_node::req_trailer, 1U);
	EXPECT_EQ(last_node::req_eom, 1U);

	auto has_connection = last_node::request->has([](const http::http_request::header_t& h) { return utils::icompare(h.first,"Connection"); });
	auto has_keep_alive = last_node::request->has([](const http::http_request::header_t& h) { return utils::icompare(h.first,"Keep-Alive"); });
	auto has_public = last_node::request->has([](const http::http_request::header_t& h) { return utils::icompare(h.first,"Public"); });
	auto has_proxy_authenticate = last_node::request->has([](const http::http_request::header_t& h) { return utils::icompare(h.first,"Proxy-Authenticate"); });
	auto has_transfer_encoding = last_node::request->has([](const http::http_request::header_t& h) { return utils::icompare(h.first,"Transfer-Encoding"); });
	auto has_upgrade = last_node::request->has([](const http::http_request::header_t& h) { return utils::icompare(h.first,"Upgrade"); });
	auto has_good_enough = last_node::request->has([](const http::http_request::header_t& h) { return utils::icompare(h.first,"GoodEnough..."); });

	EXPECT_FALSE(has_connection);
	EXPECT_FALSE(has_keep_alive);
	EXPECT_FALSE(has_public);
	EXPECT_FALSE(has_proxy_authenticate);
	EXPECT_FALSE(has_transfer_encoding);
	EXPECT_FALSE(has_upgrade);
	EXPECT_TRUE(has_good_enough);
}

TEST_F(header_filter_test, 100_not_implemented)
{
	auto init_fn = [this](boost::asio::io_service& ios)
	{
		preset::init_thread_local();
		first_node::res_stop_fn = preset::stop_thread_local;

		http::http_request req;
		req.hostname("hostname");
		req.channel(http::proto_version::HTTP20);
		req.header("expect", "100-conTinuE");

		ch = make_unique_chain<node_interface, first_node, nodes::header_filter, last_node>();
		ch->on_request_preamble(std::move(req));
	};

	service::locator::service_pool().run(init_fn);

	// check the filtering
	EXPECT_FALSE(last_node::request.is_initialized());
	EXPECT_TRUE( first_node::err == INTERNAL_ERROR(errors::http_error_code::not_implemented) ) << first_node::err.code();
}

TEST_F(header_filter_test, chunked_not_implemented)
{
	auto init_fn = [this](boost::asio::io_service& ios)
	{
		preset::init_thread_local();

		http::http_request req;
		req.hostname(("hostname"));
		req.header("transfer-encoding", "");

		ch = make_unique_chain<node_interface, first_node, nodes::header_filter, blocked_node>();
		ch->on_request_preamble(std::move(req));
	};

	service::locator::service_pool().run(init_fn);
	EXPECT_TRUE(first_node::err == INTERNAL_ERROR(errors::http_error_code::not_implemented) ) << first_node::err.code();
}

TEST_F(header_filter_test, max_forwards_0)
{
	auto init_fn = [this](boost::asio::io_service& ios)
	{
		preset::init_thread_local();

		http::http_request req;
		req.method(HTTP_OPTIONS);
		req.header(http::hf_maxforward, "0");

		ch = make_unique_chain<node_interface, first_node, nodes::header_filter, blocked_node>();
		ch->on_request_preamble(std::move(req));
	};

	service::locator::service_pool().run(init_fn);
	EXPECT_TRUE(first_node::response.is_initialized());
	EXPECT_TRUE(first_node::response->has(http::hf_allow, "GET, HEAD, POST, OPTIONS, PUT, DELETE, PATCH" ) );
	EXPECT_EQ(first_node::res_eom, 1U);
}

TEST_F(header_filter_test, max_forwards_1)
{
	auto init_fn = [this](boost::asio::io_service& ios)
	{
		preset::init_thread_local();

		http::http_request req;
		req.method(HTTP_OPTIONS);
		req.header(http::hf_maxforward, "1");

		ch = make_unique_chain<node_interface, first_node, nodes::header_filter, last_node>();
		ch->on_request_preamble(std::move(req));
		ch->on_request_finished();
	};

	service::locator::service_pool().run(init_fn);
	ASSERT_TRUE(last_node::request.is_initialized());
	EXPECT_EQ(std::string(last_node::request->serialize()),
		"OPTIONS  \r\ncontent-length: 0\r\nmax-forwards: 0\r\n\r\n");
}

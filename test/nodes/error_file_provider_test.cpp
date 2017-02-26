#include "../src/requests_manager/error_file_provider.h"
#include "common.h"

namespace
{
using namespace test_utils;
struct error_file_provider_test : public preset::test {};
}

TEST_F(error_file_provider_test, not_matching_host)
{
	const auto error_host = utils::hostname_from_url("https://wronghost.com");

	http::http_request req;
	req.hostname(error_host.c_str());
	req.method(HTTP_DELETE); // this is a forbidden method

	ch = make_unique_chain<node_interface, first_node, nodes::error_file_provider, last_node>();
	ch->on_request_preamble(std::move(req));
	ch->on_request_body({});
	ch->on_request_trailer({}, {});
	ch->on_request_finished();

	EXPECT_TRUE(last_node::request.is_initialized());
	EXPECT_EQ(last_node::req_body, 1);
	EXPECT_EQ(last_node::req_trailer, 1);
	EXPECT_EQ(last_node::req_eom, 1);

	EXPECT_TRUE(first_node::response.is_initialized());
	EXPECT_EQ(first_node::res_body, 1);
	EXPECT_EQ(first_node::res_trailer, 1);
	EXPECT_EQ(first_node::res_eom, 1);
}

TEST_F(error_file_provider_test, matching_host)
{
	const auto error_host = utils::hostname_from_url(service::locator::configuration().get_error_host());

	http::http_request req;
	req.hostname(error_host.c_str());
	req.method(HTTP_DELETE); // this is a forbidden method

	ch = make_unique_chain<node_interface, first_node, nodes::error_file_provider, blocked_node>();
	ch->on_request_preamble(std::move(req));
	ch->on_request_body({});
	ch->on_request_trailer({}, {});
	ch->on_request_finished();

	EXPECT_FALSE(first_node::response.is_initialized());
	EXPECT_EQ(first_node::res_body, 0);
	EXPECT_EQ(first_node::res_trailer, 0);
	EXPECT_EQ(first_node::res_eom, 0);
}

TEST_F(error_file_provider_test, empty_path)
{
	auto init_fn = [this](boost::asio::io_service& ios)
	{
		preset::init_thread_local();
		const auto error_host = utils::hostname_from_url(service::locator::configuration().get_error_host());

		http::http_request req;
		req.hostname(error_host.c_str());
		req.method(HTTP_GET); // this is an allowed method
		req.path(dstring{}); // empty path, forbidden should be answered

		ch = make_unique_chain<node_interface, first_node, nodes::error_file_provider, blocked_node>();
		ch->on_request_preamble(std::move(req));
		ch->on_request_body({});
		ch->on_request_trailer({}, {});
		ch->on_request_finished();

		first_node::res_stop_fn = [this, &ios]()
		{
			ASSERT_TRUE(first_node::response.is_initialized());
			EXPECT_EQ(first_node::response->status_code(),(uint16_t)errors::http_error_code::forbidden);
			EXPECT_EQ(first_node::res_body, 1);
			EXPECT_EQ(first_node::res_eom, 1);

			ios.post( [this](){ch.reset();} );
		};
	};

	service::locator::service_pool().run(init_fn);
}

TEST_F(error_file_provider_test, forbidden_path)
{
	auto init_fn = [this](boost::asio::io_service& ios)
	{
		preset::init_thread_local();
		const auto error_host = utils::hostname_from_url(service::locator::configuration().get_error_host());

		http::http_request req;
		req.hostname(error_host.c_str());
		req.method(HTTP_GET); // this is an allowed method
		req.path(("/../try_dots")); // forbidden path for security reasons

		ch = make_unique_chain<node_interface, first_node, nodes::error_file_provider, blocked_node>();
		ch->on_request_preamble(std::move(req));
		ch->on_request_body({});
		ch->on_request_trailer({}, {});
		ch->on_request_finished();

		first_node::res_stop_fn = [this, &ios]()
		{
			ASSERT_TRUE(first_node::response.is_initialized());
			EXPECT_EQ(first_node::response->status_code(),(uint16_t)errors::http_error_code::forbidden);
			EXPECT_EQ(first_node::res_body, 1);
			EXPECT_EQ(first_node::res_eom, 1);

			ios.post( [this](){ch.reset();} );
		};
	};

	service::locator::service_pool().run(init_fn);
}

TEST_F(error_file_provider_test, not_found_path)
{
	auto init_fn = [this](boost::asio::io_service& ios)
	{
		preset::init_thread_local();
		const auto error_host = utils::hostname_from_url(service::locator::configuration().get_error_host());

		http::http_request req;
		req.hostname(error_host.c_str());
		req.method(HTTP_GET); // this is an allowed method
		req.path(("/pasqualinosettebellezze")); // forbidden path for security reasons

		ch = make_unique_chain<node_interface, first_node, nodes::error_file_provider, blocked_node>();
		ch->on_request_preamble(std::move(req));
		ch->on_request_body({});
		ch->on_request_trailer({}, {});
		ch->on_request_finished();

		first_node::res_stop_fn = [this, &ios]()
		{
			EXPECT_TRUE( first_node::err == INTERNAL_ERROR(errors::http_error_code::not_found) ) << first_node::err.code();
			ios.post( [this](){ch.reset();} );
		};
	};

	service::locator::service_pool().run(init_fn);
}

TEST_F(error_file_provider_test, options)
{
	auto init_fn = [this](boost::asio::io_service& ios)
	{
		preset::init_thread_local();
		const auto error_host = utils::hostname_from_url(service::locator::configuration().get_error_host());

		http::http_request req;
		req.hostname(error_host.c_str());
		req.method(HTTP_OPTIONS); // this is an allowed method

		ch = make_unique_chain<node_interface, first_node, nodes::error_file_provider, last_node>();
		ch->on_request_preamble(std::move(req));
		ch->on_request_finished();

		first_node::res_stop_fn = [this, &ios]()
		{
			EXPECT_FALSE(last_node::request.is_initialized());
			EXPECT_EQ(last_node::req_body, 0);
			EXPECT_EQ(last_node::req_trailer, 0);
			EXPECT_EQ(last_node::req_eom, 0);
			ASSERT_TRUE(first_node::response.is_initialized());
			EXPECT_EQ(first_node::response->status_code(), 200);
			EXPECT_TRUE(first_node::response->has(http::hf_allow, "GET, HEAD,OPTIONS"));
			EXPECT_EQ(first_node::res_eom, 1);
			EXPECT_FALSE(first_node::err);

			ios.post( [this](){ch.reset();} );
		};
	};

	service::locator::service_pool().run(init_fn);
}

TEST_F(error_file_provider_test, found_path)
{
	auto init_fn = [this](boost::asio::io_service& ios)
	{
		preset::init_thread_local();
		const auto error_host = utils::hostname_from_url(service::locator::configuration().get_error_host());

		http::http_request req;
		req.hostname(error_host.c_str());
		req.method(HTTP_GET); // this is an allowed method
		req.path("/images/bg.jpg"); // existing, allowed file

		ch = make_unique_chain<node_interface, first_node, nodes::error_file_provider, last_node>();
		ch->on_request_preamble(std::move(req));
		ch->on_request_body({});
		ch->on_request_trailer({}, {});
		ch->on_request_finished();

		first_node::res_stop_fn = [this, &ios]()
		{
			EXPECT_FALSE(last_node::request.is_initialized());
			EXPECT_EQ(last_node::req_body, 0);
			EXPECT_EQ(last_node::req_trailer, 0);
			EXPECT_EQ(last_node::req_eom, 0);
			ASSERT_TRUE(first_node::response.is_initialized());
			EXPECT_EQ(first_node::response->status_code(), 200);
			EXPECT_GE(first_node::res_body, 1);
			EXPECT_EQ(first_node::res_eom, 1);
			EXPECT_FALSE(first_node::err);

			ios.post( [this](){ch.reset();} );
		};
	};

	service::locator::service_pool().run(init_fn);
}


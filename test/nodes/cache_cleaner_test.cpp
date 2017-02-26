#include <gtest/gtest.h>
#include "../../src/requests_manager/cache_cleaner.h"
#include "common.h"
#include "../../src/cache/policies/fifo.h"
#include "../../src/cache/cache.h"

using namespace test_utils;
struct cache_cleaner_test : public preset::test{
protected:
	void SetUp() override
	{
		preset::test::SetUp();
		cache<fifo_policy>::get_instance(UINT32_MAX, service::locator::configuration().cache_path())->clear_all();
	}

};

TEST_F(cache_cleaner_test, not_matching_req)
{
	http::http_request req1;
	req1.method(http_method::HTTP_GET);
	auto ch = make_unique_chain<node_interface, first_node, nodes::cache_cleaner, last_node>();
	ch->on_request_preamble(std::move(req1));
	ASSERT_TRUE(last_node::request); //it passed
	last_node::reset();
	auto ch2 = make_unique_chain<node_interface, first_node, nodes::cache_cleaner, last_node>();
	http::http_request req2;
	req2.method(http_method::HTTP_POST);
	req2.path("/cache/clear");
	ch2->on_request_preamble(std::move(req2));
	ASSERT_TRUE(last_node::request); //it passed
	last_node::reset();
	auto ch3 = make_unique_chain<node_interface, first_node, nodes::cache_cleaner, last_node>();
	http::http_request req3;
	req3.method(http_method::HTTP_POST);
	req3.path("/cache/soiahsa");
	req3.hostname("ciaone.cynny.com");
	ch3->on_request_preamble(std::move(req3));
	ASSERT_TRUE(last_node::request); //it passed
	last_node::reset();
}

TEST_F(cache_cleaner_test, clear_tag)
{
	auto ch3 = make_unique_chain<node_interface, first_node, nodes::cache_cleaner, blocked_node>();
	http::http_request req3;
	req3.method(http_method::HTTP_POST);
	req3.path("/cache/tag/js/clear");
	req3.hostname("sys.cynny.com");
	ch3->on_request_preamble(std::move(req3));
	/*checking side effects!*/
	auto cptr = cache<fifo_policy>::get_instance().get();
	ASSERT_EQ(cptr->size(), 0);
	ch3->on_request_finished();
	ASSERT_TRUE(first_node::response);
	auto resp = first_node::response;
	ASSERT_EQ(resp->status_code(), 200);
}

TEST_F(cache_cleaner_test, clear_all)
{
	/** Add stuff outside */
	auto ch3 = make_unique_chain<node_interface, first_node, nodes::cache_cleaner, blocked_node>();
	http::http_request req3;
	req3.method(http_method::HTTP_POST);
	req3.path("/cache/clear");
	req3.hostname("sys.cynny.com");
	ch3->on_request_preamble(std::move(req3));
	/*checking side effects!*/
	auto cptr = cache<fifo_policy>::get_instance().get();
	ASSERT_EQ(cptr->size(), 0);
	ch3->on_request_finished();

	ASSERT_TRUE(first_node::response);
	auto resp = first_node::response;
	ASSERT_EQ(resp->status_code(), 200);
}

TEST_F(cache_cleaner_test, side_effect_tag)
{
	auto cptr = cache<fifo_policy>::get_instance(UINT32_MAX, service::locator::configuration().cache_path()).get();
	/** Fill the cache with stuff. */

	auto &io = service::locator::service_pool().get_io_service();
	boost::asio::deadline_timer t{io}, t2{io};
	t.expires_from_now(boost::posix_time::seconds(1));
	t.async_wait([&](const boost::system::error_code &ec){
		for(auto i = 100; i < 200; ++i)
		{
			auto k = std::to_string(i);
			auto v = k;
			cptr->begin_put(k, 3600, std::chrono::system_clock::now(), "js");
			cptr->put(k, v);
			cptr->end_put(k, true);
		}

		for(auto i = 200; i < 300; ++i)
		{
			auto k = std::to_string(i);
			auto v = k;
			cptr->begin_put(k, 3600, std::chrono::system_clock::now());
			cptr->put(k, v);
			cptr->end_put(k, true);
		}
		t2.expires_from_now(boost::posix_time::seconds(2));
		t2.async_wait([&](const boost::system::error_code &ec){
			auto ch3 = make_unique_chain<node_interface, first_node, nodes::cache_cleaner, blocked_node>();
			http::http_request req3;
			req3.method(http_method::HTTP_POST);
			req3.path("/cache/tag/js/clear");
			req3.hostname("sys.cynny.com");
			ch3->on_request_preamble(std::move(req3));
			/*checking side effects!*/
			ASSERT_EQ(cptr->size(), 300);
			ch3->on_request_finished();
			ASSERT_TRUE(first_node::response);
			auto resp = first_node::response;
			ASSERT_EQ(resp->status_code(), 200);
			for(auto i = 200; i < 300; ++i)
			{
				ASSERT_TRUE(cptr->has(std::to_string(i)));
			}
			for(auto i = 100; i < 200; ++i)
			{
				ASSERT_FALSE(cptr->has(std::to_string(i)));
			}
			service::locator::service_pool().allow_graceful_termination();
		});
	});

	/** But now, check also that there's nothing left!*/
	io.run();

}



TEST_F(cache_cleaner_test, side_effect_all)
{
	auto cptr = cache<fifo_policy>::get_instance(UINT32_MAX, service::locator::configuration().cache_path()).get();
	/** Fill the cache with stuff. */

	auto &io = service::locator::service_pool().get_io_service();
	boost::asio::deadline_timer t{io}, t2{io};
	t.expires_from_now(boost::posix_time::seconds(1));
	t.async_wait([&](const boost::system::error_code &ec){
		for(auto i = 100; i < 200; ++i)
		{
			auto k = std::to_string(i);
			auto v = k;
			cptr->begin_put(k, 3600, std::chrono::system_clock::now(), "js");
			cptr->put(k, v);
			cptr->end_put(k, true);
		}

		for(auto i = 200; i < 300; ++i)
		{
			auto k = std::to_string(i);
			auto v = k;
			cptr->begin_put(k, 3600, std::chrono::system_clock::now());
			cptr->put(k, v);
			cptr->end_put(k, true);
		}
		t2.expires_from_now(boost::posix_time::seconds(2));
		t2.async_wait([&](const boost::system::error_code &ec){
			auto ch3 = make_unique_chain<node_interface, first_node, nodes::cache_cleaner, blocked_node>();
			http::http_request req3;
			req3.method(http_method::HTTP_POST);
			req3.path("/cache/clear");
			req3.hostname("sys.cynny.com");
			ch3->on_request_preamble(std::move(req3));
			/*checking side effects!*/
			ASSERT_EQ(cptr->size(), 0);
			ch3->on_request_finished();
			ASSERT_TRUE(first_node::response);
			auto resp = first_node::response;
			ASSERT_EQ(resp->status_code(), 200);
			for(auto i = 100; i < 300; ++i)
			{
				ASSERT_FALSE(cptr->has(std::to_string(i)));
			}
			service::locator::service_pool().allow_graceful_termination();
		});
	});

	/** But now, check also that there's nothing left!*/
	io.run();

}
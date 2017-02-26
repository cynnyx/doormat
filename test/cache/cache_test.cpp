#include <gtest/gtest.h>
#include "../testcommon.h"
#include "../nodes/common.h"
#include "../../src/cache/cache.h"
#include "../../src/cache/policies/fifo.h"
#include "../../src/service_locator/service_initializer.h"

#include <fstream>
#include <algorithm>
#include <chrono>


using namespace test_utils;
struct cache_test : public preset::test {

	void TearDown() override
	{
		preset::test::TearDown();
		try
		{
			cynny::cynnypp::filesystem::removeDirectory("/tmp/cache/");
		}
		catch(cynny::cynnypp::filesystem::ErrorCode &ec)
		{
			LOGERROR("that's ok, an error happened: ", ec.what());
		}
	}
};


std::string generate_random_string(size_t length)
{
	static const char alphanum[] =
		"0123456789"
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz";
	std::string s;
	s.reserve(length);

	for (size_t i = 0; i < length; ++i)
	{
		char randomlySelected = alphanum[rand() % (sizeof(alphanum) - 1)];
		s += randomlySelected;
	}

	return s;
}


TEST_F(cache_test, verify_has_not)
{
	cache<fifo_policy> current_cache;

	// try with empty string
	ASSERT_FALSE(current_cache.has(""));

	// try with random strings
	for(auto i = 0; i < 1000; ++i) {
		auto key = generate_random_string(rand()%8912);
		ASSERT_FALSE(current_cache.has(key));
	}
}


TEST_F(cache_test, atomicputandget)
{
	cache<fifo_policy> current_cache;
	int count = 0;
	int sleep_count = 0;
	boost::asio::deadline_timer t{service::locator::service_pool().get_thread_io_service()};
	std::function<void(const boost::system::error_code&)> wait_fn;
	std::function<void()> generation_function;
	generation_function = [&](){
		auto key = generate_random_string(rand()%8912);
		if(!current_cache.has(key)) {
			ASSERT_TRUE(current_cache.begin_put(key, 1000, std::chrono::system_clock::now()));
			EXPECT_TRUE(current_cache.put(key, key));
			current_cache.end_put(key);

			wait_fn = [&, key](const boost::system::error_code &ec)
			{
				if(current_cache.has(key) == false && sleep_count < 2000)
				{
					++sleep_count;
					t.expires_from_now(boost::posix_time::milliseconds(5));
					t.async_wait(wait_fn);
					return;
				}
				sleep_count = 0;
				auto has = current_cache.get(key, [&, key](std::vector<uint8_t> chunk){
					ASSERT_TRUE((std::string{reinterpret_cast<const char *>(chunk.data()), chunk.size()} == key)) << count;
					++count;
					if(count < 100)
						generation_function();
					else
						service::locator::service_pool().allow_graceful_termination();
				});
				ASSERT_TRUE(has) << count;
			};
			t.expires_from_now(boost::posix_time::milliseconds(5));
			t.async_wait(wait_fn);
		}

	};
	generation_function();
	service::locator::service_pool().get_thread_io_service().run();
	ASSERT_EQ(count, 100);
}


TEST_F(cache_test, compositeputandget)
{
	cache<fifo_policy> current_cache;
	int count = 0;
	std::function<void()> generation_function;
	std::function<void(const boost::system::error_code&)> wait_fn;
	generation_function = [&](){
		auto key = generate_random_string(rand()%8912);
		if(!current_cache.has(key)) {
			ASSERT_TRUE(current_cache.begin_put(key, 1000,std::chrono::system_clock::now()));
			auto expect = generate_random_string(rand()%8912);
			EXPECT_TRUE(current_cache.put(key, (expect)));

			service::locator::service_pool().get_thread_io_service().post(
					[&, expect, key]() mutable {
						auto expect2 = generate_random_string(rand()%8912);
						EXPECT_TRUE(current_cache.put(key, (expect2)));
						current_cache.end_put(key);
						auto t = std::make_shared<boost::asio::deadline_timer>(service::locator::service_pool().get_thread_io_service());
						auto sleep_count = std::make_shared<int>(0);
						wait_fn = [&, expect, expect2, key, t, sleep_count](const boost::system::error_code &ec) mutable {
							if(current_cache.has(key) == false && (*sleep_count)++ < 2000)
							{
								t->expires_from_now(boost::posix_time::milliseconds(5));
								t->async_wait(wait_fn);
								return;
							}
							(*sleep_count) = 0;
							auto has = current_cache.get(key, [&, key, expect, expect2, t](std::vector<uint8_t> chunk) mutable {
								expect += expect2;
								ASSERT_TRUE((std::string{reinterpret_cast<const char *>(chunk.data()), chunk.size()} == expect));
								++count;
								if(count < 100) generation_function();
								else service::locator::service_pool().allow_graceful_termination();
							});
							ASSERT_TRUE(has) << count;
						};
						t->expires_from_now(boost::posix_time::milliseconds(5));
						t->async_wait(wait_fn);
					}
			);
		}

	};
	generation_function();
	service::locator::service_pool().get_thread_io_service().run();
	ASSERT_EQ(count, 100);
}


TEST_F(cache_test, ttl_with_eviction)
{
	cache<fifo_policy> cache(490);
	std::vector<std::pair<std::string, bool>> inserted_elements;
	std::string key;
	for(int i = 0; i < 100; ++i)
	{
		key = std::to_string(i);
		cache.begin_put(key, 10,std::chrono::system_clock::now());
		cache.put(key, key);
		cache.end_put(key);
		inserted_elements.push_back(std::make_pair(key, false));
	}
	/*** verify latest key was inserted*/
	for(int i = 0; i < 100; ++i)
	{
		key = std::to_string(i+100);
		cache.begin_put(key, 1024,std::chrono::system_clock::now());
		cache.put(key, key);
		cache.end_put(key);
		inserted_elements.push_back(std::make_pair(key, true));
	}

	boost::asio::deadline_timer await_fs_timer(service::locator::service_pool().get_thread_io_service());
	await_fs_timer.expires_from_now(boost::posix_time::milliseconds(5));
	int sleep_count = 0;
	std::function<void(const boost::system::error_code&ec)> await_callback;
	await_callback = [&](const boost::system::error_code &ec)
	{
		if(!cache.has(key) && sleep_count++ < 2000)
		{
			await_fs_timer.expires_from_now(boost::posix_time::milliseconds(5));
			await_fs_timer.async_wait(await_callback);
			return;
		} else
		{
			service::locator::service_pool().allow_graceful_termination();
		}
	};
	await_fs_timer.async_wait(await_callback);
	service::locator::service_pool().get_thread_io_service().run();
	LOGTRACE("going to sleep for a while to test ttl");
	sleep(11);
	LOGTRACE("waking up and evicting");
	cache.evict();
	for(auto &el: inserted_elements)
	{
		ASSERT_TRUE(cache.has(el.first) == el.second);
	}
	//do something
}

TEST_F(cache_test, ttl_with_insert)
{
	cache<fifo_policy> cache(489);
	std::vector<std::pair<std::string, bool>> inserted_elements;
	std::string key;
	for(int i = 0; i < 100; ++i)
	{
		key = std::to_string(i);
		cache.begin_put(key, 10,std::chrono::system_clock::now());
		cache.put(key, key);
		cache.end_put(key);
		inserted_elements.push_back(std::make_pair(key, false));
	}

boost::asio::deadline_timer await_fs_timer(service::locator::service_pool().get_thread_io_service());
await_fs_timer.expires_from_now(boost::posix_time::milliseconds(5));
int sleep_count = 0;
std::function<void(const boost::system::error_code&ec)> await_callback;
await_callback = [&](const boost::system::error_code &ec)
{
	if(!cache.has(key) && sleep_count++ < 2000)
	{
		await_fs_timer.expires_from_now(boost::posix_time::milliseconds(5));
		await_fs_timer.async_wait(await_callback);
		return;
	} else
	{
		service::locator::service_pool().allow_graceful_termination();

		LOGTRACE("cache size in bytes is ", cache.size());
		LOGTRACE("going to sleep for a while to test ttl");

		await_fs_timer.expires_from_now(boost::posix_time::milliseconds(11000));
		await_fs_timer.async_wait([&](const boost::system::error_code &ec){
			std::cout << "waking up and evicting" << std::endl;
			for(int i = 0; i < 100; ++i)
			{
				key = std::to_string(i+100);
				cache.begin_put(key, 1024,std::chrono::system_clock::now());
				cache.put(key, key);
				cache.end_put(key);
				inserted_elements.push_back(std::make_pair(key, true));
			}

			await_fs_timer.expires_from_now(boost::posix_time::milliseconds(100));
			await_fs_timer.async_wait([&](const boost::system::error_code &ec)
			{
				LOGTRACE("cache size is ", cache.size());
				LOGTRACE("verification");
				for(auto &el: inserted_elements)
				{
					ASSERT_EQ(cache.has(el.first), el.second);
				}
				LOGTRACE("cache size is ", cache.size());
			});
		});

	}
};
await_fs_timer.async_wait(await_callback);
service::locator::service_pool().get_thread_io_service().run();

}


TEST_F(cache_test, eviction_without_ttl)
{
	cache<fifo_policy> cache(302);
	std::vector<std::pair<std::string, bool>> inserted_elements;

	std::string key;
	for(int i = 0; i < 100; ++i)
	{
		key = std::to_string(i);
		cache.begin_put(key, 100,std::chrono::system_clock::now());
		cache.put(key, key);
		cache.end_put(key);
		inserted_elements.push_back(std::make_pair(key, false));
	}

	boost::asio::deadline_timer wait_fs(service::locator::service_pool().get_thread_io_service());
	int wait_attempts{0};
	wait_fs.expires_from_now(boost::posix_time::milliseconds(10));
	std::function<void(const boost::system::error_code &)> wait_fun;
	wait_fun = [&](const boost::system::error_code&ec)
	{
		if(!cache.has(key) && wait_attempts++ < 2000)
		{
			wait_fs.expires_from_now(boost::posix_time::milliseconds(5));
			wait_fs.async_wait(wait_fun);
			return;
		}

		std::cout << "cache size is: " << cache.size() << std::endl;

		for(int i = 0; i < 100; ++i)
		{
			key = std::to_string(i+100);
			cache.begin_put(key, 100,std::chrono::system_clock::now());
			cache.put(key, key);
			cache.end_put(key);
			inserted_elements.push_back(std::make_pair(key, true));
		}

		wait_fs.expires_from_now(boost::posix_time::milliseconds(1000));
		wait_fs.async_wait([&](const boost::system::error_code &ec){
			for(auto &el: inserted_elements)
			{
				ASSERT_EQ(cache.has(el.first), el.second);
			}
			service::locator::service_pool().allow_graceful_termination();
		});
	};
	wait_fs.async_wait(wait_fun);
	service::locator::service_pool().get_thread_io_service().run();
}


TEST_F(cache_test, tagging)
{
	cache<fifo_policy> cache(4096);
	//inserting element in cache.
	std::string tag = "numbers";
	std::string key;
	for(int i = 0; i < 100; ++i)
	{
		key = std::to_string(i+100);
		cache.begin_put(key, 100,std::chrono::system_clock::now(), tag);
		cache.put(key, key);
		cache.end_put(key);
	}
	int counter = 0;
	boost::asio::deadline_timer wait_fs(service::locator::service_pool().get_thread_io_service());
	std::function<void(const boost::system::error_code &)> wait_fun;
	wait_fs.expires_from_now(boost::posix_time::milliseconds(10));
	wait_fun = [&](const boost::system::error_code &ec){
		if(!cache.has(key) && ++counter < 2000)
		{
			wait_fs.expires_from_now(boost::posix_time::milliseconds(5));
			wait_fs.async_wait(wait_fun);
			LOGTRACE("waiting");
			return;
		}
		for(int i = 0; i < 100; ++i)
		{
			auto key = std::to_string(i+100);
			ASSERT_TRUE(cache.has(key));
		}
		cache.clear_tag(tag);

		for(int i = 0; i < 100; ++i)
		{
			auto key = std::to_string(i+100);
			ASSERT_FALSE(cache.has(key));
		}

		service::locator::service_pool().allow_graceful_termination();

	};
	wait_fs.async_wait(wait_fun);
	service::locator::service_pool().get_thread_io_service().run();
}


TEST_F(cache_test, put_twice)
{
	cache<fifo_policy> c(4096);

	auto key = generate_random_string(500);
	ASSERT_TRUE(c.begin_put(key, 100,std::chrono::system_clock::now()));
	c.put(key, key);
	ASSERT_FALSE(c.begin_put(key, 2000,std::chrono::system_clock::now()));
	c.end_put(key);
	int counter = 0;
	boost::asio::deadline_timer wait_fs(service::locator::service_pool().get_thread_io_service());
	std::function<void(const boost::system::error_code &)> wait_fun;
	wait_fs.expires_from_now(boost::posix_time::milliseconds(10));
	wait_fun = [&](const boost::system::error_code &ec)
	{
		if (!c.has(key) && ++counter < 2000)
		{
			wait_fs.expires_from_now(boost::posix_time::milliseconds(5));
			wait_fs.async_wait(wait_fun);
			return;
		}

		service::locator::service_pool().allow_graceful_termination();
	};
	wait_fs.async_wait(wait_fun);
	ASSERT_FALSE(c.begin_put(key, 2000,std::chrono::system_clock::now()));

	service::locator::service_pool().get_thread_io_service().run();
}


TEST_F(cache_test, put_and_get_empty)
{
	cache<fifo_policy> c(400);
	std::string key;
	std::string value = "968465434987984654564";
	ASSERT_TRUE(c.begin_put(key, 100,std::chrono::system_clock::now()));
	ASSERT_FALSE(c.begin_put(key, 100,std::chrono::system_clock::now()));
	c.put(key, value);
	c.end_put(key);
	int counter = 0;
	boost::asio::deadline_timer wait_fs(service::locator::service_pool().get_thread_io_service());
	std::function<void(const boost::system::error_code &)> wait_fun;
	wait_fs.expires_from_now(boost::posix_time::milliseconds(10));
	wait_fun = [&](const boost::system::error_code &ec)
	{
		if (!c.has(key) && ++counter < 2000)
		{
			wait_fs.expires_from_now(boost::posix_time::milliseconds(5));
			wait_fs.async_wait(wait_fun);
			return;
		}
		c.get(key, [&](std::vector<uint8_t> data){
			std::string retrieved{reinterpret_cast<const char *>(data.data()), data.size()};
			ASSERT_EQ(retrieved, value);
			service::locator::service_pool().allow_graceful_termination();
		});
	};
	wait_fs.async_wait(wait_fun);
	ASSERT_FALSE(c.begin_put(key, 2000,std::chrono::system_clock::now()));

	service::locator::service_pool().get_thread_io_service().run();

}

/*
 * put and invalidate an element
 */
TEST_F(cache_test, invalidate)
{
	const std::string key = "cicciopasticcio";
	const auto ttl_seconds = 1000;
	auto& io = service::locator::service_pool().get_thread_io_service();
	boost::asio::basic_waitable_timer<std::chrono::steady_clock> timer(io);
	const auto wait_time = std::chrono::milliseconds(20);
	size_t wait_count{};
	const size_t max_wait_count = 100;

	cache<fifo_policy> c;
	ASSERT_TRUE(c.begin_put(key, ttl_seconds,std::chrono::system_clock::now()));
	c.put(key, "cicciobistecca");
	c.end_put(key, true);

	std::function<void(const boost::system::error_code &)> wait_fun;
	timer.expires_from_now(wait_time);
	wait_fun = [&](const boost::system::error_code& ec)
	{
		if(!c.has(key) && ++wait_count < max_wait_count)
		{
			timer.expires_from_now(wait_time);
			timer.async_wait(wait_fun);
			return;
		}

		ASSERT_TRUE(c.has(key));
		c.invalidate(key);
		ASSERT_FALSE(c.has(key));
		service::locator::service_pool().allow_graceful_termination();
	};
	timer.async_wait(wait_fun);

	io.run();
}

/*
 * putting and invalidating an element several times
 */
TEST_F(cache_test, invalidate_and_reput)
{
	const std::string key = "cicciopasticcio";
	const auto ttl_seconds = 1000;
	auto& io = service::locator::service_pool().get_thread_io_service();
	boost::asio::basic_waitable_timer<std::chrono::steady_clock> timer(io);
	const auto wait_time = std::chrono::milliseconds(20);
	size_t wait_count{};
	const size_t max_wait_count = 100;
	const size_t times = 20;
	size_t iter{};

	cache<fifo_policy> c;
	ASSERT_TRUE(c.begin_put(key, ttl_seconds,std::chrono::system_clock::now()));
	c.put(key, "cicciobistecca");
	c.end_put(key, true);

	std::function<void(const boost::system::error_code &)> wait_fun;
	timer.expires_from_now(wait_time);
	wait_fun = [&](const boost::system::error_code& ec)
	{
		if(!c.has(key) && ++wait_count < max_wait_count)
		{
			timer.expires_from_now(wait_time);
			timer.async_wait(wait_fun);
			return;
		}

		ASSERT_TRUE(c.has(key));
		c.invalidate(key);
		ASSERT_FALSE(c.has(key));

		if(++iter >= times)
			service::locator::service_pool().allow_graceful_termination();
		else
		{
			wait_count = 0;
			ASSERT_TRUE(c.begin_put(key, ttl_seconds,std::chrono::system_clock::now()));
			c.put(key, "cicciobistecca");
			c.end_put(key, true);
			timer.expires_from_now(wait_time);
			timer.async_wait(wait_fun);
		}
	};
	timer.async_wait(wait_fun);

	io.run();
}

/*
 * We expect that when the cach max size is 0,
 * every put will cause the eviction of all other
 * elements inside the cache
 */
TEST_F(cache_test, max_size_zero)
{
	const std::string key = "cicciopasticcio";
	const auto ttl_seconds = 1000;
	auto& io = service::locator::service_pool().get_thread_io_service();
	boost::asio::basic_waitable_timer<std::chrono::steady_clock> timer(io);
	const auto wait_time = std::chrono::milliseconds(20);
	size_t wait_count{};
	const size_t max_wait_count = 20;

	cache<fifo_policy> c(0);
	ASSERT_TRUE(c.begin_put(key, ttl_seconds,std::chrono::system_clock::now()));
	c.put(key, "cicciobistecca");
	c.end_put(key, true);

	std::function<void(const boost::system::error_code &)> wait_fun;
	timer.expires_from_now(wait_time);
	wait_fun = [&](const boost::system::error_code& ec)
	{
		if(!c.has(key) && ++wait_count < max_wait_count)
		{
			timer.expires_from_now(wait_time);
			timer.async_wait(wait_fun);
			return;
		}

		ASSERT_TRUE(c.has(key));

		// inserting a new element will evict the old one
		c.begin_put(key+key, ttl_seconds,std::chrono::system_clock::now());
		c.put(key+key, "cicciobistecca");
		c.end_put(key+key, true);
		ASSERT_FALSE(c.has(key));

		// wait for the fs to finish writing the new element, and then exit
		wait_fun = [&](const boost::system::error_code& ec)
		{
			if(c.has(key+key))
				service::locator::service_pool().allow_graceful_termination();
			else
			{
				timer.expires_from_now(wait_time);
				timer.async_wait(wait_fun);
			}
		};
		timer.expires_from_now(wait_time);
		timer.async_wait(wait_fun);
	};

	timer.async_wait(wait_fun);

	io.run();
}


/*
 * we expect that performing a get just after an end_put
 * will result in an empty buffer passed to the get callback
 */
TEST_F(cache_test, get_just_after_commit)
{
	const std::string key = "cicciopasticcio";
	const auto ttl_seconds = 1000;
	auto& io = service::locator::service_pool().get_thread_io_service();
	boost::asio::basic_waitable_timer<std::chrono::steady_clock> timer(io);
	const auto wait_time = std::chrono::milliseconds(20);

	cache<fifo_policy> c;
	ASSERT_TRUE(c.begin_put(key, ttl_seconds,std::chrono::system_clock::now()));
	c.put(key, "cicciobistecca");
	c.end_put(key, true);
	c.get(key, [](std::vector<uint8_t> data)
	{
		EXPECT_TRUE(data.empty());
	});

	std::function<void(const boost::system::error_code &)> wait_fun;
	timer.expires_from_now(wait_time);
	wait_fun = [&](const boost::system::error_code& ec)
	{
		if(c.has(key))
			service::locator::service_pool().allow_graceful_termination();
		else
		{
			timer.expires_from_now(wait_time);
			timer.async_wait(wait_fun);
		}
	};

	timer.async_wait(wait_fun);

	io.run();
}


TEST_F(cache_test, put)
{
	const std::string key = "cicciopasticcio";
	const auto ttl_seconds = 1000;

	cache<fifo_policy> c;
	EXPECT_FALSE(c.put(key, "cicciobistecca")); // no begin put yet
	ASSERT_TRUE(c.begin_put(key, ttl_seconds,std::chrono::system_clock::now())); // ok
	ASSERT_FALSE(c.begin_put(key, ttl_seconds,std::chrono::system_clock::now())); // can't perform begin_put twice
	EXPECT_TRUE(c.put(key, "cicciobistecca")); // ok
	c.end_put(key, false);
	EXPECT_FALSE(c.put(key, "cicciobistecca")); // can't perform put after end_put
}


/*
 * we put an element inside the cache with a tag,
 * we wait for its ttl to expire, then we remove the tag.
 * we expect no side effects (exceptions or memory
 * courruptions checkable by e.g. asan) will occur.
 */
TEST_F(cache_test, ttl_then_tag_eviction)
{
	const std::string key = "cicciopasticcio";
	const std::string tag = "shtaug";
	const auto ttl_seconds = 1;
	auto& io = service::locator::service_pool().get_thread_io_service();
	boost::asio::basic_waitable_timer<std::chrono::steady_clock> timer(io);
	const auto wait_time = std::chrono::seconds(2);

	cache<fifo_policy> c;
	ASSERT_TRUE(c.begin_put(key, ttl_seconds,std::chrono::system_clock::now(), tag));
	c.put(key, "cicciobistecca");
	c.end_put(key, true);

	std::function<void(const boost::system::error_code &)> wait_fun;
	wait_fun = [&](const boost::system::error_code& ec)
	{
		EXPECT_FALSE(c.has(key));
		EXPECT_NO_THROW(c.clear_tag(tag));
		EXPECT_FALSE(c.has(key));
		service::locator::service_pool().allow_graceful_termination();
	};
	timer.expires_from_now(wait_time);
	timer.async_wait(wait_fun);

	io.run();
}

/*
 * we put an element inside the cache with a tag,
 * we remove the tag and then wait the ttl to expire.
 * we expect no side effects (exceptions or memory
 * courruptions checkable by e.g. asan) will occur.
 */
TEST_F(cache_test, tag_then_ttl_eviction)
{
	const std::string key = "cicciopasticcio";
	const std::string tag = "shtaug";
	const auto ttl_seconds = 1;
	auto& io = service::locator::service_pool().get_thread_io_service();
	boost::asio::basic_waitable_timer<std::chrono::steady_clock> timer(io);
	const auto wait_time = std::chrono::seconds(2);

	cache<fifo_policy> c;
	ASSERT_TRUE(c.begin_put(key, ttl_seconds,std::chrono::system_clock::now(), tag));
	c.put(key, "cicciobistecca");
	c.end_put(key, true);
	c.clear_tag(tag);
	EXPECT_FALSE(c.has(key));

	std::function<void(const boost::system::error_code &)> wait_fun;
	wait_fun = [&](const boost::system::error_code& ec)
	{
		EXPECT_FALSE(c.has(key));
		service::locator::service_pool().allow_graceful_termination();
	};
	timer.expires_from_now(wait_time);
	timer.async_wait(wait_fun);

	io.run();
}

/*
 * we put an element inside the cache with a tag,
 * we remove the tag before the end_put has been
 * called. we expect no side effects (exceptions or memory
 * courruptions checkable by e.g. asan) will occur.
 */
TEST_F(cache_test, invalidate_before_commit)
{
	const std::string key = "cicciopasticcio";
	const auto ttl_seconds = 1000;
	auto& io = service::locator::service_pool().get_thread_io_service();
	boost::asio::basic_waitable_timer<std::chrono::steady_clock> timer(io);
	const auto wait_time = std::chrono::seconds(1);
	//size_t wait_count{};
	//const size_t max_wait_count = 100;

	cache<fifo_policy> c;
	ASSERT_TRUE(c.begin_put(key, ttl_seconds,std::chrono::system_clock::now()));
	c.put(key, "cicciobistecca");
	c.invalidate(key);
	c.end_put(key, true);
	EXPECT_FALSE(c.has(key));

	std::function<void(const boost::system::error_code &)> wait_fun;
	wait_fun = [&](const boost::system::error_code& ec)
	{
		EXPECT_FALSE(c.has(key));
		service::locator::service_pool().allow_graceful_termination();
	};
	timer.expires_from_now(wait_time);
	timer.async_wait(wait_fun);

	io.run();
}


/*
 * we put an element inside the cache with a tag,
 * we remove the tag before the end_put has been
 * called. we expect no side effects (exceptions or memory
 * courruptions checkable by e.g. asan) will occur.
 */
TEST_F(cache_test, tag_eviction_before_commit)
{
	const std::string key = "cicciopasticcio";
	const std::string tag = "shtaug";
	const auto ttl_seconds = 1000;
	auto& io = service::locator::service_pool().get_thread_io_service();
	boost::asio::basic_waitable_timer<std::chrono::steady_clock> timer(io);
	const auto wait_time = std::chrono::milliseconds(20);
	size_t wait_count{};
	const size_t max_wait_count = 100;

	cache<fifo_policy> c;
	ASSERT_TRUE(c.begin_put(key, ttl_seconds,std::chrono::system_clock::now(), tag));
	c.put(key, "cicciobistecca");
	c.clear_tag(tag);
	c.end_put(key, true);
	EXPECT_FALSE(c.has(key));

	std::function<void(const boost::system::error_code &)> wait_fun;
	wait_fun = [&](const boost::system::error_code& ec)
	{
		if(!c.has(key) && ++wait_count < max_wait_count)
		{
			timer.expires_from_now(wait_time);
			timer.async_wait(wait_fun);
			return;
		}

		EXPECT_TRUE(c.has(key));
		service::locator::service_pool().allow_graceful_termination();
	};
	timer.expires_from_now(wait_time);
	timer.async_wait(wait_fun);

	io.run();
}

/*
 * we put an element inside the cache with ttl = 0.
 * we expect not to find it after a second within the cache
 */
TEST_F(cache_test, ttl_zero)
{
	const std::string key = "cicciopasticcio";
	const auto ttl_seconds = 0;
	auto& io = service::locator::service_pool().get_thread_io_service();
	boost::asio::basic_waitable_timer<std::chrono::steady_clock> timer(io);
	const auto wait_time = std::chrono::seconds(1);

	cache<fifo_policy> c;
	c.begin_put(key, ttl_seconds,std::chrono::system_clock::now());
	c.put(key, "cicciobistecca");
	c.end_put(key, true);
	EXPECT_FALSE(c.has(key));

	std::function<void(const boost::system::error_code &)> wait_fun;
	wait_fun = [&](const boost::system::error_code& ec)
	{
		EXPECT_FALSE(c.has(key));
		service::locator::service_pool().allow_graceful_termination();
	};
	timer.expires_from_now(wait_time);
	timer.async_wait(wait_fun);

	io.run();
}

/*
 * we trigger a get of an element from the cache just
 * before its ttl will expire; we expect to get the
 * element inside the callback, even if the callback
 * will be invoked after the ttl has expired
 */
TEST_F(cache_test, read_and_ttl)
{
	const std::string key = "cicciopasticcio";
	const std::string payload = "cicciobistecca";
	const auto ttl_seconds = 1;
	auto& io = service::locator::service_pool().get_thread_io_service();
	boost::asio::basic_waitable_timer<std::chrono::steady_clock> timer(io);
	const auto wait_time = std::chrono::milliseconds(20);
	size_t wait_count{};
	const size_t max_wait_count = 20;

	cache<fifo_policy> c(0);
	ASSERT_TRUE(c.begin_put(key, ttl_seconds,std::chrono::system_clock::now()));
	c.put(key, payload);
	c.end_put(key, true);

	std::function<void(const boost::system::error_code &)> wait_fun;
	timer.expires_from_now(wait_time);
	wait_fun = [&](const boost::system::error_code& ec)
	{
		if(!c.has(key) && ++wait_count < max_wait_count)
		{
			timer.expires_from_now(wait_time);
			timer.async_wait(wait_fun);
			return;
		}

		// we schedule a get from the cache
		bool has = c.get(key, [&](std::vector<uint8_t> data)
		{
			EXPECT_TRUE(std::equal(data.begin(), data.end(), payload.begin()));
			EXPECT_EQ(data.size(), payload.size());
			service::locator::service_pool().allow_graceful_termination();
		});

		ASSERT_TRUE(has);
		sleep(2);
	};

	timer.async_wait(wait_fun);

	io.run();
}



TEST_F(cache_test, clear_all)
{
	auto total_size = 0;
	cache<fifo_policy> c(300000);
	std::vector<std::string> keyvector;
	auto& io = service::locator::service_pool().get_thread_io_service();
	for(int i = 0; i < 100; ++i)
	{
		auto k = generate_random_string(200);
		auto v = generate_random_string(300);
		if(c.begin_put(k, 3600, std::chrono::system_clock::now()))
		{
			c.put(k, v);
			c.end_put(k, true);
			total_size += v.size();
			keyvector.push_back(k);
		}
	}

	/** Inserted everything; now wait a while*/
	boost::asio::deadline_timer t{io};
	t.expires_from_now(boost::posix_time::seconds(1));
	t.async_wait([&](const boost::system::error_code &ec){
		auto total_size_freed = c.clear_all();
		ASSERT_EQ(total_size_freed, total_size);
		for(auto &k : keyvector)
		{
			ASSERT_FALSE(c.has(k));
		}
		ASSERT_EQ(c.size(), 0);
		service::locator::service_pool().allow_graceful_termination();
	});

	io.run();
}

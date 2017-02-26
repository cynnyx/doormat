#include "../src/cache/cache.h"
#include "utils/measurements.h"
#include "../test/testcommon.h"
#include "../test/nodes/common.h"
#include "../src/cache/policies/fifo.h"
#include "../src/service_locator/service_initializer.h"
#include <gtest/gtest.h>


std::string generate_random_string(size_t length) {
	static const char alphanum[] =
			"0123456789"
					"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
					"abcdefghijklmnopqrstuvwxyz";
	std::string s;
	s.reserve(length);

	for (size_t i = 0; i < length; ++i) {
		char randomlySelected = alphanum[rand() % (sizeof(alphanum) - 1)];
		s += randomlySelected;
	}

	return s;
}

using namespace test_utils;

static std::vector<std::string> requested_uris;

struct cache_perf_test : public preset::test {
protected:
	virtual void SetUp() override
	{
		preset::test::SetUp();
		/** Load file*/
		if(requested_uris.empty()) {
			std::cout << "LOADING!" << std::endl;
			std::ifstream infile("../resources/logs/accesslog_purified.txt");
			std::string tmp;
			while(std::getline(infile, tmp)) requested_uris.push_back(tmp);

			std::ifstream infile2("../resources/logs/accesscynnyspace_purified.txt");
			while(std::getline(infile2, tmp)) requested_uris.push_back(tmp);
			//int generator = 2758787;
			std::random_shuffle(requested_uris.begin(), requested_uris.end());
		}
		std::cout << "FINISHED LOADING" << std::endl;
	}

	virtual void TearDown() override
	{
		preset::test::TearDown();
		cynny::cynnypp::filesystem::removeDirectory("/tmp/cache/");
	}
};


TEST_F(cache_perf_test, put)
{
	measurement ts_put("[CACHE] Ts put (ms)");
	measurement l_put("[CACHE] Latency put (ms)");
	measurement bw_put("[CACHE] Byte time put (s/MB)");
	cache<fifo_policy> current_cache;
	std::string key;
	std::chrono::high_resolution_clock::time_point beginning = std::chrono::high_resolution_clock::now();
	auto begin = requested_uris.begin();
	int i;
	double total_bytes_inserted = 0;
	for(i = 0; i < 500000 && begin != requested_uris.end(); ++i)
	{
		if(current_cache.has(*begin) || !current_cache.begin_put(*begin, 564546,std::chrono::system_clock::now()))
		{
			--i;
			++begin;
			continue;
		}
		key = *begin;
		++begin;
		total_bytes_inserted+=key.size();
		current_cache.put(key, key);
		current_cache.end_put(key);
	}

	std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();
	auto time = std::chrono::duration_cast<std::chrono::milliseconds>(end - beginning);
	ts_put.put(((double)time.count())/i);
	ts_put.push_average();

	int count = 0;
	boost::asio::deadline_timer wait_fs(service::locator::service_pool().get_thread_io_service());
	wait_fs.expires_from_now(boost::posix_time::milliseconds(5));
	std::function<void(const boost::system::error_code &ec)> wait_fun;
	wait_fun = [&, key](const boost::system::error_code &ec){
		if(!current_cache.has(key) && count++ < 1000000)
		{
			wait_fs.expires_from_now(boost::posix_time::milliseconds(5));
			wait_fs.async_wait(wait_fun);
			return;
		}
		double latency =std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - beginning).count();
		l_put.put(latency/i);
		l_put.push_average();
		// bytes / ms --> bytes * 1000 / ms = bytes/s --> bytes *1000 / ms * 1000 * 1000 -> bytes /ms *1000 = MBytes/s
		bw_put.put(1/(total_bytes_inserted/(latency*1000)));
		bw_put.push_average();

		service::locator::service_pool().allow_graceful_termination();
	};
	wait_fs.async_wait(wait_fun);
	service::locator::service_pool().get_thread_io_service().run();
}



TEST_F(cache_perf_test, has_not_seq) {
	cache<fifo_policy> current_cache;
	measurement ts_has_not("[CACHE] TS negative has (us)");
	measurement ts_global_has_not("[CACHE] TS global has not (us)");
	measurement ts_put("[CACHE] Ts put (ms)");
	int i;
	auto begin = requested_uris.begin();
	std::string key;
	auto put_begin = std::chrono::system_clock::now();
	for(i = 0; i < 500000 && begin != requested_uris.end(); ++i)
	{
		if(current_cache.has(*begin) || !current_cache.begin_put(*begin, 564546,std::chrono::system_clock::now()))
		{
			--i;
			++begin;
			continue;
		}
		key = *begin;
		++begin;
		current_cache.put(key, "ciao");
		current_cache.end_put(key);
	}
	auto put_end = std::chrono::system_clock::now();

	ts_put.put(((double)std::chrono::duration_cast<std::chrono::milliseconds>(put_end - put_begin).count())/i);
	ts_put.push_average();
	int count = 0;
	boost::asio::deadline_timer wait_fs(service::locator::service_pool().get_thread_io_service());
	wait_fs.expires_from_now(boost::posix_time::milliseconds(5));
	std::function<void(const boost::system::error_code &ec)> wait_fun;
	wait_fun = [&, key](const boost::system::error_code &ec){
		if(!current_cache.has(key) && count++ < 1000000)
		{
			wait_fs.expires_from_now(boost::posix_time::milliseconds(5));
			wait_fs.async_wait(wait_fun);
			return;
		}
		service::locator::service_pool().allow_graceful_termination();
	};
	wait_fs.async_wait(wait_fun);

	service::locator::service_pool().get_thread_io_service().run();

	key = generate_random_string(400);
	std::chrono::high_resolution_clock::time_point beginning = std::chrono::high_resolution_clock::now();
	for(auto i = 0; i < 2<<20; ++i)
	{
		key[rand()%400] = 'c';
		if(i % 400 == 0) key = generate_random_string(400);
		ASSERT_FALSE(current_cache.has(key));
	}

	std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();

	auto time = std::chrono::duration_cast<std::chrono::microseconds>(end - beginning);

	ts_has_not.put(((double)time.count())/(2<<20));
	ts_global_has_not.put(time.count());

	ts_has_not.push_average();
	ts_global_has_not.push_average();
}

TEST_F(cache_perf_test, has_mixed)
{
	cache<fifo_policy> current_cache;
	measurement ts_has_mixed("[CACHE] TS mixed (us)");
	measurement ts_put("[CACHE] Ts put (ms)");
	int i;
	auto begin = requested_uris.begin();
	std::string key;
	auto put_begin = std::chrono::system_clock::now();
	for(i = 0; i < 500000 && begin != requested_uris.end(); ++i)
	{

		if(current_cache.has(*begin) || !current_cache.begin_put(*begin, 564546,std::chrono::system_clock::now()))
		{
			--i;
			++begin;
			continue;
		}
		key = *begin;
		++begin;
		current_cache.put(key, "ciao");
		current_cache.end_put(key);
	}
	auto put_end = std::chrono::system_clock::now();

	ts_put.put(((double)std::chrono::duration_cast<std::chrono::milliseconds>(put_end - put_begin).count())/i);
	ts_put.push_average();
	int count = 0;
	boost::asio::deadline_timer wait_fs(service::locator::service_pool().get_thread_io_service());
	wait_fs.expires_from_now(boost::posix_time::milliseconds(5));
	std::function<void(const boost::system::error_code &ec)> wait_fun;
	wait_fun = [&, key](const boost::system::error_code &ec){
		if(!current_cache.has(key) && count++ < 1000000)
		{
			wait_fs.expires_from_now(boost::posix_time::milliseconds(5));
			wait_fs.async_wait(wait_fun);
			return;
		}
		service::locator::service_pool().allow_graceful_termination();
	};
	wait_fs.async_wait(wait_fun);

	service::locator::service_pool().get_thread_io_service().run();

	std::chrono::high_resolution_clock::time_point beginning = std::chrono::high_resolution_clock::now();
	auto insertend = begin;
	auto itbegin = requested_uris.begin();
	i = 0;
	while(i < 2<<20 && itbegin != insertend)
	{
		int r = rand()%10;
		if(r > 1) current_cache.has(generate_random_string(400));
		else current_cache.has(*itbegin);
		++i;
		++itbegin;
		if(itbegin == insertend) itbegin = requested_uris.begin();
	}
	std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();
	double time = std::chrono::duration_cast<std::chrono::microseconds>(end - beginning).count();
	//ts_has_positive_global.put(time);
	ts_has_mixed.put(time/i);
	ts_has_mixed.push_average();
	//ts_has_positive_global.push_average();
}

TEST_F(cache_perf_test, has_seq){
	struct rep{};
	measurement ts_put("[CACHE] Ts put (ms)");
	measurement ts_has_positive("[CACHE] Ts has positive (us)");
	measurement ts_has_positive_global("[CACHE] Ts has positive global(us)");
	cache<fifo_policy> current_cache;
	std::vector<std::string> inserted_keys;
	auto begin = requested_uris.begin();
	int i = 0;
	std::string key;
	auto put_begin = std::chrono::system_clock::now();
	for(i = 0; i < 500000 && begin != requested_uris.end(); ++i)
	{
		if(current_cache.has(*begin) || !current_cache.begin_put(*begin, 564546,std::chrono::system_clock::now()))
		{
			--i;
			++begin;
			continue;
		}
		key = *begin;
		++begin;
		current_cache.put(key, "ciao");
		current_cache.end_put(key);
	}
	auto put_end = std::chrono::system_clock::now();
	double put_time = std::chrono::duration_cast<std::chrono::milliseconds>(put_end - put_begin).count();
	ts_put.put(put_time/i);
	ts_put.push_average();

	int count = 0;
	boost::asio::deadline_timer wait_fs(service::locator::service_pool().get_thread_io_service());
	wait_fs.expires_from_now(boost::posix_time::milliseconds(5));
	std::function<void(const boost::system::error_code &ec)> wait_fun;
	wait_fun = [&, key](const boost::system::error_code &ec){
		if(!current_cache.has(key) && count++ < 1000000)
		{
			wait_fs.expires_from_now(boost::posix_time::milliseconds(5));
			wait_fs.async_wait(wait_fun);
			return;
		}
		std::cout << "finished writing" << std::endl;
		service::locator::service_pool().allow_graceful_termination();
	};
	wait_fs.async_wait(wait_fun);

	service::locator::service_pool().get_thread_io_service().run();

	std::chrono::high_resolution_clock::time_point beginning = std::chrono::high_resolution_clock::now();
	auto insertend = begin;
	auto itbegin = requested_uris.begin();
	i = 0;
	while(i < 2<<20 && itbegin != insertend)
	{
	ASSERT_TRUE(current_cache.has(*itbegin)) << *itbegin;
	++i;
	++itbegin;
	if(itbegin == insertend) itbegin = requested_uris.begin();
	}
	std::chrono::high_resolution_clock::time_point end = std::chrono::high_resolution_clock::now();
	double time = std::chrono::duration_cast<std::chrono::microseconds>(end - beginning).count();
	ts_has_positive_global.put(time);
	ts_has_positive.put(time/i);
	ts_has_positive.push_average();
	ts_has_positive_global.push_average();
}



TEST_F(cache_perf_test, get) {
	struct rep{};
	measurement get{"[CACHE] Latency Get (us) "};
	cache<fifo_policy> current_cache;
	std::vector<std::string> inserted_keys;
	auto begin = requested_uris.begin();
	int i = 0;
	std::string key;
	std::string value = generate_random_string(8192);
	//auto put_begin = std::chrono::system_clock::now();
	for(i = 0; i < 500000 && begin != requested_uris.end(); ++i)
	{
		if(current_cache.has(*begin) || !current_cache.begin_put(*begin, 564546,std::chrono::system_clock::now()))
		{
			--i;
			++begin;
			continue;
		}
		key = *begin;
		++begin;
		current_cache.put(key, value);
		current_cache.end_put(key);
	}
	//auto put_end = std::chrono::system_clock::now();
	//double put_time = std::chrono::duration_cast<std::chrono::milliseconds>(put_end - put_begin).count();
	auto gbegin = requested_uris.begin();
	std::function<void()> perform_all_gets;
	perform_all_gets = [&](){
		if(gbegin == requested_uris.end())
		{
			service::locator::service_pool().allow_graceful_termination();
			return;
		}
		auto get_begin = std::chrono::system_clock::now();
		if(current_cache.has(*gbegin)) {
			current_cache.get(*gbegin, [&, get_begin](std::vector<uint8_t> data){
				ASSERT_TRUE(data.size() > 0);
				auto get_end = std::chrono::system_clock::now();
				auto duration = std::chrono::duration_cast<std::chrono::microseconds>(get_end - get_begin).count();
				get.put(duration);
				++gbegin;
				service::locator::service_pool().get_thread_io_service().post(perform_all_gets);
			});
		} else
		{
			++gbegin;
			service::locator::service_pool().get_thread_io_service().post(perform_all_gets);
		}
	};
	int count = 0;
	boost::asio::deadline_timer wait_fs(service::locator::service_pool().get_thread_io_service());
	wait_fs.expires_from_now(boost::posix_time::milliseconds(5));
	std::function<void(const boost::system::error_code &ec)> wait_fun;
	wait_fun = [&, key](const boost::system::error_code &ec){
		if(!current_cache.has(key) && count++ < 1000000)
		{
			wait_fs.expires_from_now(boost::posix_time::milliseconds(5));
			wait_fs.async_wait(wait_fun);
			return;
		}
		std::cout << "finished writing" << std::endl;
		service::locator::service_pool().get_thread_io_service().post(perform_all_gets);
	};
	wait_fs.async_wait(wait_fun);

	service::locator::service_pool().get_thread_io_service().run();

	get.push_average();
}




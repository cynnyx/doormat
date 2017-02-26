#include <gtest/gtest.h>
#include "../src/service_locator/service_initializer.h"
#include "../src/utils/json.hpp"

#include <thread>
#include <chrono>
#include <atomic>
#include <random>
#include <vector>

#include "boost/asio.hpp"

using namespace stats;


class thread_container
{
	std::vector<std::thread> v;

public:
	template<typename F>
	thread_container(size_t num, F&& f)
		: v(num)
	{
		for(auto& t : v)
		{
			std::thread tmp(std::forward<F>(f));
			t.swap(tmp);
		}
	}

	void join()
	{
		for(auto& t : v)
			t.join();
	}
};

TEST(StatsManager, SingleProducerConstInput)
{
	stats_manager sm(1);
	sm.register_handler();
	sm.start();

	for( uint8_t i=0; i<100; ++i )
	{
		sm.enqueue(round_trip_latency, 1 );
		sm.enqueue(request_number, 1 );
	}

	std::this_thread::sleep_for(stats_manager::c_collection_period * 1.5);

	EXPECT_DOUBLE_EQ( sm.get_value(round_trip_latency), 1.0 );
	EXPECT_DOUBLE_EQ( sm.get_value(request_number), 100.0);
}

TEST(StatsManager, TwoProducersRandomInput)
{
	std::atomic<uint32_t> req_count { 0 };
	std::atomic<uint32_t> avg_value_rtt { 0 };
	std::atomic_bool stop { false };
	stats_manager sm(2);

	auto prod_fn = [&sm, &stop, &req_count, &avg_value_rtt]()
	{
		sm.register_handler();
		while( !stop )
		{
			std::random_device rd;
			for( uint8_t j=0; j< stats_manager::c_queue_length/100 ; ++j )
			{
				auto rtt = rd()%1000;
				sm.enqueue(request_number, 1 );
				sm.enqueue(round_trip_latency, rtt );
				req_count.fetch_add(1,std::memory_order_relaxed);
				avg_value_rtt.fetch_add(rtt,std::memory_order_relaxed);
			}
			std::this_thread::sleep_for(std::chrono::milliseconds( rd()%( stats_manager::c_collection_period.count()/10 ) ));
		}
	};

	std::thread producer1( prod_fn );
	std::thread producer2( prod_fn );
	sm.start();
	std::random_device rd;
	std::this_thread::sleep_for(std::chrono::milliseconds( rd()%1000 ));
	stop = true ;
	producer1.join();
	producer2.join();

	std::this_thread::sleep_for(stats_manager::c_collection_period * 1.5);
	EXPECT_DOUBLE_EQ( sm.get_value(request_number), double(req_count) );
	EXPECT_DOUBLE_EQ( sm.get_value(round_trip_latency), double(avg_value_rtt)/req_count );
}

TEST(StatsManager, ManyProducersRandomInput)
{
	size_t num_threads = 50;
	std::atomic<uint32_t> req_count { 0 };
	std::atomic<uint32_t> avg_value_rtt { 0 };
	std::atomic_bool stop { false };
	stats_manager sm(num_threads);

	auto prod_fn = [&sm, &stop, &req_count, &avg_value_rtt]()
	{
		sm.register_handler();
		while( !stop )
		{
			std::random_device rd;
			for( uint8_t j=0; j< stats_manager::c_queue_length/100 ; ++j )
			{
				auto rtt = rd()%1000;
				sm.enqueue(request_number, 1 );
				sm.enqueue(round_trip_latency, rtt );
				req_count.fetch_add(1,std::memory_order_relaxed);
				avg_value_rtt.fetch_add(rtt,std::memory_order_relaxed);
			}
			std::this_thread::sleep_for(std::chrono::milliseconds( rd()%( stats_manager::c_collection_period.count()/10 ) ));
		}
	};

	thread_container producers(num_threads, prod_fn);
	sm.start();

	std::random_device rd;
	std::this_thread::sleep_for(std::chrono::milliseconds( rd()%1000 ));
	stop = true ;
	producers.join();

	std::this_thread::sleep_for(stats_manager::c_collection_period * 1.5);
	EXPECT_DOUBLE_EQ( sm.get_value(request_number), double(req_count) );
	EXPECT_DOUBLE_EQ( sm.get_value(round_trip_latency), double(avg_value_rtt)/req_count );
}

TEST(StatsManager, Reset)
{
	stats_manager sm(1);
	sm.register_handler();
	sm.start();

	sm.enqueue(request_number, 1 );
	sm.enqueue(round_trip_latency, 0.5 );
	std::this_thread::sleep_for(std::chrono::milliseconds(1100));
	auto rn = sm.get_value(request_number);
	auto rtl = sm.get_value(round_trip_latency);

	EXPECT_DOUBLE_EQ( rn, 1);
	EXPECT_DOUBLE_EQ( rtl, 0.5 );

	sm.reset();
	rn = sm.get_value(request_number);
	rtl = sm.get_value(round_trip_latency);
	EXPECT_DOUBLE_EQ( rn, 0 );
	//After reset this should be NaN
	EXPECT_TRUE( rtl != rtl );
}

TEST(StatsManager, JsonResponse)
{
	stats_manager sm(1);
	service::initializer::load_configuration("./etc/doormat/doormat.test.config");
	service::initializer::init_services();
	sm.register_handler();
	sm.start();
	sm.reset();

	auto parsed = nlohmann::json::parse(sm.serialize());

	EXPECT_TRUE( parsed.size() );

	auto stats = parsed["stats"];
	for( size_t i = first_gauge; i < overall_counter_number; ++i )
	{
		auto t = static_cast<stat_type>(i);
		double value = sm.get_value(t);
		if( value != value )
			EXPECT_EQ( stats[stat_type_2_name.at(t)], nullptr );
		else
			EXPECT_EQ( double(stats[stat_type_2_name.at(t)]), value );
	}

	service::initializer::terminate_services();
}

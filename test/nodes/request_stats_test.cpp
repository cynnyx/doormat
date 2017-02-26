#include "../src/requests_manager/request_stats.h"
#include "common.h"

#include <chrono>
#include <numeric>

namespace
{
using namespace test_utils;
struct request_stats_test : public preset::test {};
}


TEST_F(request_stats_test, usage)
{
	static constexpr size_t iteration {23};
	service::locator::stats_manager().register_handler();
	service::locator::stats_manager().start();

	ch = make_unique_chain<node_interface, first_node, nodes::request_stats, last_node>();

	std::vector<size_t> v;
	for(size_t i = 0; i < iteration; ++i)
	{
		ch->on_request_preamble({});

		const auto lapse = std::chrono::milliseconds(i);
		std::this_thread::sleep_for(lapse);

		if(i % 2)
			ch->on_request_finished();
		else
			ch->on_request_canceled({});

		v.push_back(lapse.count());
	}

	std::this_thread::sleep_for(stats::stats_manager::c_collection_period * 2);

	auto mean = std::accumulate(v.begin(), v.end(), 0) * 1000000.0 / v.size();
	auto times = service::locator::stats_manager().get_value(stats::request_number);

	EXPECT_EQ(times, iteration);
	// 20% of error margin... we are not testing accuracy for sure :P
	EXPECT_NEAR(service::locator::stats_manager().get_value(stats::round_trip_latency), mean, mean * 0.2);
}

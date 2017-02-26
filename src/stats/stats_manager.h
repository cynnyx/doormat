#ifndef DOORMAT_STATSMANAGER_H
#define DOORMAT_STATSMANAGER_H

#include <cassert>
#include <cstdint>
#include <thread>
#include <mutex>
#include <array>
#include <atomic>
#include <vector>
#include <map>
#include <chrono>

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/mean.hpp>
#include <boost/accumulators/statistics/min.hpp>
#include <boost/accumulators/statistics/max.hpp>

#include <boost/lockfree/spsc_queue.hpp>
#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>


namespace stats
{

enum stat_type
{
	//Gauge(s)
	first_gauge = 0,
	round_trip_latency = first_gauge, //--> Insert gauges below this line
	board_response_latency,
	access_log_time,
	overall_gauges_number,

	//Counter(s)
	first_counter = overall_gauges_number,
	request_number = first_counter, // --> Insert counters below this line
	failed_req_number,
	cache_requests_arrived,
	cache_hits,
	//the following value of the enum must remain the last, as it is used in order to multiplex the data in the stats_manager.
	overall_counter_number
};

static const std::map<stat_type, std::string> stat_type_2_name
{
	{ round_trip_latency, "RoundTripLatency" },
	{ board_response_latency, "BoardResponseLatency"},
	{ access_log_time, "AccessLogWritingTime" },
	{ request_number, "RequestNumber" },
	{ failed_req_number, "FailedRequestNumber" },
	{ cache_requests_arrived, "RequestNumberAsSeenByCache"},
	{ cache_hits, "RequestsServedByCache"}
};

class stats_manager;

std::ostream& operator<<(std::ostream& os, stats_manager& sm);

static const size_t gauges_number{ overall_gauges_number - first_gauge };
static const size_t counter_number{ overall_counter_number - first_counter };
static const size_t uninitialized_thread_id{ 0xFFFFFFFF };

bool is_gauge( const stat_type& type ) noexcept;
bool is_counter( const stat_type& type ) noexcept;

class stats_manager
{
	using stat_counter = std::pair< stat_type, double>;
	using stats_queue = boost::lockfree::spsc_queue<stat_counter>;

	class gauge_container
	{
		using stats = typename boost::accumulators::features < boost::accumulators::tag::mean
			, boost::accumulators::tag::count, boost::accumulators::tag::min, boost::accumulators::tag::max	>;

		using gauge_counter = typename boost::accumulators::accumulator_set< double, stats >;
		std::array< gauge_counter, gauges_number > inner_container;

	public:
		void reset()
		{
			for( size_t i = 0; i< inner_container.size(); ++i )
				inner_container[i] = gauge_counter();
		}

		void append( const stat_type type, const double& value )
		{
			assert( type < gauges_number );
			inner_container[type]( value );
		}

		double get_mean_value( const stat_type type ) const
		{
			assert( type < gauges_number );
			return boost::accumulators::mean(inner_container[type]);
		}

		double get_max_value( const stat_type type ) const
		{
			assert( type < gauges_number );
			return boost::accumulators::max(inner_container[type]);
		}

		double get_min_value( const stat_type type ) const
		{
			assert( type < gauges_number );
			return boost::accumulators::min(inner_container[type]);
		}

		double get_count_value( const stat_type type ) const
		{
			assert( type < gauges_number );
			return boost::accumulators::count(inner_container[type]);
		}
	};

	class counter_container
	{
		std::array< double, counter_number > inner_container;

	public:
		counter_container()
			: inner_container{}
		{}

		void reset()
		{
			for( size_t i = 0; i< inner_container.size(); ++i )
				inner_container[i] = 0;
		}

		void append( const stat_type type, double value )
		{
			assert( type < counter_number );
			inner_container[type] += value;
		}

		double get_count_value( const stat_type type ) const
		{
			assert( type < counter_number );
			return inner_container[type];
		}
	};

	bool stopped{true};

	const size_t max_handlers;
	std::atomic<size_t> registered_thrds_g_count { 0 };

	gauge_container gauge_counters;
	counter_container incremental_counters;

	mutable std::mutex mtx;
	std::vector< std::unique_ptr< stats_queue > > queues;

	boost::asio::io_service evb;
	std::unique_ptr<boost::asio::io_service::work> keep_alive;
	boost::asio::steady_timer timer;
	std::unique_ptr<std::thread> collector_thrd;

	void collect_stats(const boost::system::error_code& ec) noexcept;

public:
	static const std::chrono::milliseconds c_collection_period;
	static const uint32_t c_queue_length;

	stats_manager(const size_t threads);
	virtual ~stats_manager() noexcept;

	void register_handler( ) noexcept;
	bool is_handler_registered( ) const noexcept;
	void start() noexcept;
	void stop() noexcept;
	void reset() noexcept;
	void enqueue( const stat_type type, const double& value );

	double get_value( const stat_type type ) const;

	std::string serialize() const;
};

}

#endif // DOORMAT_STATSMANAGER_H

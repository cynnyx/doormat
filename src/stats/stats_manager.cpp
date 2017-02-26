#include "stats_manager.h"

#include <functional>
#include <sstream>
#include <chrono>

#include "../utils/log_wrapper.h"
#include "../utils/json.hpp"

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>

#include "../utils/likely.h"

using namespace std;
using namespace boost;

namespace stats
{

thread_local size_t producer_thread_id { uninitialized_thread_id };

const std::chrono::milliseconds stats_manager::c_collection_period { 1000 };
const uint32_t stats_manager::c_queue_length { 5000 };

bool is_gauge( const stat_type& type ) noexcept { return type >= first_gauge && type < overall_gauges_number; }
bool is_counter( const stat_type& type ) noexcept { return type >= first_counter && type < overall_counter_number; }

/**
 * @brief operator<<
 */
std::ostream& operator<<(std::ostream& os, stats_manager& sm)
{
	for( size_t i = first_gauge; i < overall_counter_number; ++i )
		os<< stat_type_2_name.at((stat_type)i) <<":"<< sm.get_value((stat_type)i)<<"\n";
	return os;
}

stats_manager::stats_manager(const size_t threads)
	: max_handlers( threads )
	, queues( max_handlers )
	, keep_alive( new boost::asio::io_service::work( evb ) )
	, timer( evb )
{}

stats_manager::~stats_manager() noexcept
{
	if(!stopped)
		stop();
}

/**
 *
 */
void stats_manager::start() noexcept
{
	collector_thrd.reset(new std::thread([this]()
	{
		//Schedule first event
		timer.expires_from_now( c_collection_period );
		timer.async_wait( std::bind( &stats_manager::collect_stats, this, std::placeholders::_1 ) );

		//Start loop
		evb.run();

		LOGTRACE("Exiting collector thread");
	}));

	stopped = false;
}

void stats_manager::stop() noexcept
{
	stopped = true;

	boost::system::error_code ec;
	timer.cancel(ec);

	if(ec)
		LOGTRACE("Error canceling the timer", ec.message());

	keep_alive.reset();
	evb.stop();

	if(collector_thrd->joinable())
		collector_thrd->join();
}

/**
 * @brief collectStats
 */
void stats_manager::enqueue( const stat_type type, const double& value )
{
	if( LIKELY( is_handler_registered( ) && producer_thread_id < max_handlers ) )
		queues[producer_thread_id]->push( stat_counter(type, value) );
	else
		LOGTRACE("Failed to empty enqueue: handler not registered or invalid id");
}

/**
 * @brief collectStats
 */
void stats_manager::collect_stats( const boost::system::error_code& ec) noexcept
{
	if( ! stopped )
	{
		// if the timer has been canceled (allegedly in the destructor), don't schedule the timer anymore
		if( !ec )
		{
			timer.expires_from_now( c_collection_period );
			timer.async_wait( std::bind( &stats_manager::collect_stats, this, std::placeholders::_1 ) );
		}

		if(registered_thrds_g_count == max_handlers )
		{
			stat_counter sample;
			std::lock_guard<std::mutex> lck{mtx};
			for( auto& queue : queues )
			{
				while ( queue && queue->pop(sample) )
				{
					if( is_gauge( sample.first ) )
					{
						gauge_counters.append( sample.first, sample.second );
					}
					else
					{
						assert( is_counter( sample.first ) );
						incremental_counters.append( (stat_type)(sample.first - overall_gauges_number), sample.second );
					}
				}
			}
		}
	}
}

/**
 * @brief registerHandler
 */
void stats_manager::register_handler( ) noexcept
{
	//Prevent registering multiple times the same handler
	if( ! is_handler_registered( ) )
	{
		assert( registered_thrds_g_count < max_handlers );
		producer_thread_id = registered_thrds_g_count++;
		queues[producer_thread_id].reset( new stats_queue( c_queue_length ) );
		LOGTRACE("New StatsQueue ", producer_thread_id , " registered to thread ", std::this_thread::get_id());
	}
}

/**
 * @brief isHandlerRegistered
 */
bool stats_manager::is_handler_registered( ) const noexcept
{
	return registered_thrds_g_count != 0 && producer_thread_id != uninitialized_thread_id;
}

/**
 * @brief reset
 */
void stats_manager::reset() noexcept
{
	std::lock_guard<std::mutex> lck{mtx};
	gauge_counters.reset();
	incremental_counters.reset();
}

/**
 * @brief getMeanValue
 * @param type : the stat type that identifies the value to retrieve
 */
double stats_manager::get_value(const stat_type type ) const
{
	if( is_gauge( type ) )
	{
		std::lock_guard<std::mutex> lck{mtx};
		return gauge_counters.get_mean_value( type );
	}
	else
	{
		assert( is_counter( type ) );

		std::lock_guard<std::mutex> lck{mtx};
		return incremental_counters.get_count_value( (stat_type)(type - overall_gauges_number) );
	}
}

// ------------- stats serialization ----------------

/**
 * @brief serialize
 */
std::string stats_manager::serialize() const
{
	nlohmann::json ret;

	// serialize statistics
	nlohmann::json stats;
	for( size_t i = first_gauge; i < overall_counter_number; ++i )
	{
		auto t = static_cast<stat_type>(i);
		stats[stat_type_2_name.at( t )] = get_value( t ); // TODO: format double with 2 digits
	}

	ret["stats"] = stats;
	return ret.dump(4);
}

}

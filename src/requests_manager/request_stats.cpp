#include "request_stats.h"
#include "../service_locator/service_locator.h"
#include "../stats/stats_manager.h"

namespace nodes
{

void request_stats::on_request_preamble( http::http_request&& preamble )
{
	start = std::chrono::high_resolution_clock::now();
	service::locator::stats_manager().enqueue ( stats::stat_type::request_number, 1.0 );
	base::on_request_preamble ( std::move ( preamble ) );
}

void request_stats::on_end_of_message()
{
	create_duration();
	base::on_end_of_message();
}

void request_stats::on_error( const errors::error_code& ec )
{
	create_duration();
	base::on_error( ec );
}

void request_stats::create_duration() const
{
	auto end = std::chrono::high_resolution_clock::now();
	auto latency = end - start;

	uint64_t nanoseconds = std::chrono::nanoseconds(latency).count();
	/// @note nanoseconds here are a problem to cast to double because doubles are not very precise over 2^53
	service::locator::stats_manager().enqueue ( stats::stat_type::round_trip_latency, static_cast<double>( nanoseconds ) );
}

}

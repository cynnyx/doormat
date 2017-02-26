#include "board_map.h"

#include "../constants.h"
#include "../utils/json.hpp"
#include "../utils/log_wrapper.h"
#include "../log/format.h"
#include "../service_locator/service_locator.h"
#include "../configuration/configuration_wrapper.h"
#include "../configuration/configuration_exception.h"

#include <fstream>
#include <vector>
#include <cstdint>
#include <chrono>
#include <algorithm>
#include <boost/regex.hpp>

using namespace std;
using namespace boost;

namespace routing
{

board_map::board_map()
{
	auto& configuration = service::locator::configuration();

	string rmap = configuration.get_route_map();

	fstream file( rmap, ios_base::in );
	string line;
	regex re("(\\s*)(server)(\\s*)\\[(.*)\\]:(\\d+)(\\s*)max_fails=(\\d+)(\\s*)fail_timeout=(\\d+)s;");
	regex comment("^\\s+#"); // Comments
	while ( getline( file, line ) && destinations.size() < UINT32_MAX )
	{
		smatch what;
		if ( regex_search ( line, what, comment ) )
			continue;

		if ( regex_search ( line, what, re ) )
		{
			board cboard( what[4], stoi( what[5] ), stoi( what[7] ), stoi( what[9] ) );
			LOGDEBUG(this, " added new destination: ", cboard.ipv6_address, ":", cboard.port);
			destinations.emplace_back( cboard );
		}
	}

	if ( destinations.empty() )
		throw configuration::configuration_exception("No destination configured");

	if ( randomization() )
		random_shuffle( destinations.begin(), destinations.end() );

	int16_t counter = 0;
	for ( auto&& b : destinations )
		ip_index.insert( std::make_pair<address, int32_t> ( address{b.ipv6_address, b.port}, counter++ ) );

	index.store( 0);
}

abstract_destination_provider::address board_map::retrieve_destination( const abstract_destination_provider::address& addr ) const noexcept
{
	try
	{
		int32_t current_index = ip_index.at( addr );
		const board& r = destinations[ current_index ];

		return abstract_destination_provider::address{r.ipv6_address, r.port};
	}
	catch ( std::out_of_range& e )
	{
		return address{"", 0};
	}
}

abstract_destination_provider::address board_map::retrieve_destination() const noexcept
{
	int attempt = destinations.size();

	while ( attempt-- > 0 )
	{
		uint16_t local_index = index.fetch_add(1) % destinations.size();
		std::chrono::system_clock::time_point now = std::chrono::system_clock::now();

		const board& destination = destinations[local_index];
		if ( destination.next_retry.load() > std::chrono::system_clock::to_time_t( now ) )
			continue;

		if ( destination.failed_times.load() == destination.max_fail )
			continue;

		if ( ! destination.enable )
			continue;

		return abstract_destination_provider::address { destination.ipv6_address, destination.port };
	}

	return abstract_destination_provider::address{"", 0};
}

void board_map::destination_failed( const abstract_destination_provider::address& ip ) noexcept
{
	try
	{

		int32_t current_index = ip_index.at( ip );
		LOGTRACE("ip ", ip.ipv6(), " port", ip.port(), " found at index ", current_index);
		destinations[ current_index ].failed_now();
	}
	catch ( std::out_of_range& e )
	{
		LOGERROR(e.what());
	}
}

void board_map::destination_worked( const abstract_destination_provider::address& ip ) noexcept
{
	try
	{
		int32_t current_index = ip_index.at( ip );
		destinations[ current_index ].worked_now();
	}
	catch ( std::out_of_range& e )
	{
		LOGERROR(e.what());
	}
}

bool board_map::disable_destination( const address& ip ) noexcept
{
	try
	{
		int32_t current_index = ip_index.at( ip );
		destinations[ current_index ].disable();
		return true;
	}
	catch ( std::out_of_range& e )
	{
		LOGERROR(e.what());
		return false;
	}
}

std::vector<board> board_map::boards() const noexcept
{
	return destinations;
}

/**
 * @brief serialize
 */
std::string board_map::serialize() const
{
	static auto board_status = [](const routing::board& b, std::chrono::time_point<std::chrono::system_clock> now)
	{
		/*
			 * fields will be stored alphabetically no matter of the order of insertion;
			 * this is because the json object uses an std::map under the hood;
			 * with some fix to the library, one could use an std::unordered_map,
			 * but the order of insertion would not be preserved too (I tried).
			 */
		nlohmann::json ret;
		ret["ip"] = b.ipv6_address;
		ret["fail_timeout"] = b.fail_timeout;
		ret["failed_times"] = b.failed_times.load();
		ret["max_fail"] = b.max_fail;
		auto nxt_ret = std::chrono::system_clock::from_time_t(b.next_retry.load());
		ret["next_retry"] = logging::format::http_header_date(nxt_ret);
		ret["port"] = b.port;
		ret["status"] = nxt_ret > now ? "down" : "up";
		return ret;
	};

	const auto now = std::chrono::system_clock::now();
	nlohmann::json bs;
	for(auto& d : destinations)
		bs.push_back(board_status(d, now));

	nlohmann::json ret;
	ret["boards_status"] = bs;
	return ret.dump(4);
}

}//routing

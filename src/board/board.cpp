#include "board.h"
#include <chrono>

using namespace std;

namespace routing
{

board::board( const std::string& address_, uint16_t port_,
	uint16_t max_fail_, uint16_t fail_timeout_, bool enable_ ):
	ipv6_address{address_},
	port{port_},
	max_fail{max_fail_},
	fail_timeout{ fail_timeout_ },
	failed_times{0},
	next_retry{0},
	enable{enable_}{}

board::board(board&& other):
	ipv6_address{other.ipv6_address},
	port{other.port},
	max_fail{other.max_fail},
	fail_timeout{other.fail_timeout},
	failed_times{other.failed_times.load()},
	next_retry{other.next_retry.load()},
	enable{other.enable.load()}{}

board::board(const board& other):
	ipv6_address{other.ipv6_address},
	port{other.port},
	max_fail{other.max_fail},
	fail_timeout{other.fail_timeout}, failed_times{other.failed_times.load()},
	next_retry{other.next_retry.load()},
	enable{other.enable.load()}{}

board& board::operator=( const board& other )
{
	ipv6_address = other.ipv6_address;
	port = other.port;
	max_fail = other.max_fail;
	fail_timeout = other.fail_timeout;
	failed_times.store( other.failed_times.load() );
	next_retry.store( other.next_retry.load() );
	enable.store( other.enable );
	return *this;
}

board& board::operator=( board&& other )
{
	ipv6_address = other.ipv6_address;
	port = other.port;
	max_fail = other.max_fail;
	fail_timeout = other.fail_timeout;
	failed_times.store( other.failed_times.load() );
	next_retry.store( other.next_retry.load() );
	enable.store( other.enable );
	return *this;
}

void board::worked_now() const
{
	if ( failed_times.load() > 0 )
	{
		chrono::time_point<chrono::system_clock> epoch = chrono::system_clock::from_time_t ( 0 );
		next_retry = chrono::system_clock::to_time_t( epoch );
		failed_times.store( 0 );
	}
}

void board::failed_now() const
{
	chrono::time_point<chrono::system_clock> next_time = chrono::system_clock::now() + chrono::seconds( fail_timeout );
	next_retry = chrono::system_clock::to_time_t( next_time );
	failed_times.fetch_add( 1 );
}

void board::disable() const
{
	enable = false;
}

}

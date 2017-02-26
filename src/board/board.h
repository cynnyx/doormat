#ifndef DOOR_MAT_BOARD_H
#define DOOR_MAT_BOARD_H

#include <string>
#include <cstdint>
#include <atomic>

namespace routing
{

struct board
{
	std::string ipv6_address;
	uint16_t port;
	uint16_t max_fail;
	uint16_t fail_timeout;
	mutable std::atomic_int_least16_t failed_times; // How many times has this board failed?
	mutable std::atomic_int_least64_t next_retry;
	mutable std::atomic_bool enable;

	board( const std::string& address_, uint16_t port_,
			uint16_t max_fail_, uint16_t fail_timeout_, bool enable_ = true );
	board( board&& );
	board( const board& );
	board& operator=( const board& other );
	board& operator=( board&& other );

	void failed_now() const;
	void worked_now() const;
	void disable() const;
};

}

#endif //DOOR_MAT_BOARD_H

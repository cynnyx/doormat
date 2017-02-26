#ifndef DOOR_MAT_BOARD_MAP_H
#define DOOR_MAT_BOARD_MAP_H

#include "abstract_destination_provider.h"
#include "board.h"

#include <map>
#include <vector>
#include <string>

namespace routing
{

class board_map : public abstract_destination_provider
{
	std::vector<board> destinations;
	std::map<abstract_destination_provider::address, int32_t> ip_index;
	mutable std::atomic_int_least16_t index;
public:
	board_map();
	virtual ~board_map() = default;

	address retrieve_destination() const noexcept override;
	address retrieve_destination( const address& addr ) const noexcept override;
	std::vector<board> boards() const noexcept override;
	void destination_worked( const abstract_destination_provider::address& addr ) noexcept override;
	void destination_failed( const abstract_destination_provider::address& addr ) noexcept override;
	bool disable_destination( const address& ip ) noexcept override;
	std::string serialize() const override;
};

}

#endif //DOOR_MAT_BOARD_MAP_H

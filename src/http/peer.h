#ifndef DOOR_MAT_PEER_H
#define DOOR_MAT_PEER_H

#include <string>
#include <cstdint>

namespace http
{

class peer
{
	std::string ip_;
	std::uint16_t port_;
public:
	peer( const std::string& ip, std::uint16_t port ): ip_(ip), port_(port) {}
	peer(): ip_(""), port_(0) {}
	peer( const peer& ) = default;
	peer& operator=( const peer& ) = default;
	std::string ip() const noexcept;
	std::uint16_t port() const noexcept;

	bool valid() const noexcept { return port_ != 0 && ip_ != ""; }
};

}
#endif //DOOR_MAT_PEER_H

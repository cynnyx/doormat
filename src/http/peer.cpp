#include "peer.h"

namespace http
{
std::string peer::ip() const noexcept
{
	return ip_;
}

std::uint16_t peer::port() const noexcept
{
	return port_;
}

}

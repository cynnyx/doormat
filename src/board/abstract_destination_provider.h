#ifndef DOOR_MAT_ABSTRACT_DESTINATION_PROVIDER_H
#define DOOR_MAT_ABSTRACT_DESTINATION_PROVIDER_H

#include <string>
#include <vector>
#include <boost/asio/ip/tcp.hpp>

#include "board.h"

namespace routing
{

struct abstract_destination_provider
{
	struct address
	{
		address() {}
		address( const std::string& ip, uint16_t port_ )
		{
			boost::system::error_code ec;
			_representation = boost::asio::ip::tcp::endpoint{boost::asio::ip::address::from_string(ip, ec), port_};
			initialized = !ec;
		}
		address( const address& other ) = default;
		address& operator=( const address& other ) = default;
		address( address&& other ) = default;
		address& operator=( address&& other ) = default;
		bool operator<( const address& o ) const
		{
			return _representation < o._representation;
		} // For Map
		bool operator==( const address& o ) const { return _representation == o._representation; }

		bool is_valid() const noexcept { return (_representation.address().is_v6() || _representation.address().is_v4())&& _representation.port() > 0; }
		boost::asio::ip::tcp::endpoint _representation;
		bool initialized{false};

		boost::asio::ip::address ipv6() const { return _representation.address(); }

		void ipv6(const std::string& ipv6_str) {
			_representation.address(boost::asio::ip::address::from_string(ipv6_str));
		}

		void port(uint16_t port)
		{
			_representation.port(port);
		}
		unsigned short port() const { return _representation.port(); }

		const boost::asio::ip::tcp::endpoint & endpoint() const
		{
			return _representation;
		}
	};

	virtual address retrieve_destination () const noexcept = 0;
	virtual address retrieve_destination ( const address& ip ) const noexcept = 0;
	virtual void destination_worked( const address& ip ) noexcept = 0;
	virtual void destination_failed ( const address& ip ) noexcept = 0;
	virtual std::vector<board> boards() const noexcept = 0;
	virtual ~abstract_destination_provider() = default;

	virtual bool disable_destination( const address& ip ) noexcept = 0;
	virtual std::string serialize() const = 0;
};

}

namespace std
{

template <>
struct hash<routing::abstract_destination_provider::address>
{
	std::size_t operator()(const routing::abstract_destination_provider::address& k) const
	{
		return hash<string>{}( k.ipv6().to_string() ) ^ hash<uint16_t>{}( k.port() );
	}
};


}

#endif //DOOR_MAT_ABSTRACT_DESTINATION_PROVIDER_H

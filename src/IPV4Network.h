#ifndef IPV4NETWORK_H
#define IPV4NETWORK_H

#include <string>
#include <cstdint>

namespace network 
{
	
class IPV4Network
{
    const std::string ip;
    const std::uint8_t netbits;
    static std::uint32_t raw_ip( const std::string& ipl  );
public:
    IPV4Network( const std::string& ip_, std::uint8_t bits ): ip(ip_), netbits(bits) {}
    bool match ( const std::string& ip_ ) const;
};

}
#endif // IPV4NETWORK_H

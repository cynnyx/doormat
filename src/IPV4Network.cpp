#include "IPV4Network.h"

#include <arpa/inet.h>
#include <string.h>

namespace network 
{

std::uint32_t IPV4Network::raw_ip(const std::string& ipl)
{
    struct in_addr ipn;
    memset( &ipn, 0, sizeof(ipn) );
    inet_aton( ipl.c_str(), &ipn);
    return ntohl ( ipn.s_addr );
}

bool IPV4Network::match(const std::string& ip_) const
{
    if ( netbits == 32 )
        return ip == ip_;

    std::uint32_t current = raw_ip( ip );
    std::uint32_t other = raw_ip ( ip_ );
    std::uint32_t mask = ~0;
    mask <<= 32 - netbits;
    return ( current & mask ) == ( other & mask );
}

}

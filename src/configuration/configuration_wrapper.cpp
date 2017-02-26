#include "configuration_wrapper.h"
#include "../constants.h"
#include "../service_locator/service_common.h"
#include "../service_locator/service_locator.h"
#include "../utils/likely.h"
#include "../utils/utils.h"

#include <fstream>
#include <iostream>
#include <sys/stat.h>
#include <boost/regex.hpp>

using namespace std;
using namespace boost;
using namespace network;

// needed to avoid conflicts
using std::uint64_t;

static constexpr uint16_t default_port{443};
static constexpr uint16_t default_port_h{80};

namespace configuration
{

bool configuration_wrapper::privileged_ipv4_matcher( const string& ip ) const noexcept
{
	for ( const IPV4Network& entry : privileged_networks )
	{
		if ( entry.match( ip ) )
			return true;
	}
	return false;
}

bool configuration_wrapper::interreg_ipv4_matcher( const string& ip ) const noexcept
{
	for ( const IPV4Network& entry : interreg_networks )
	{
		if ( entry.match( ip ) )
			return true;
	}
	return false;
}

bool configuration_wrapper::comp_mime_matcher(const std::string& mt) const noexcept
{
	for ( const auto& entry : compressed_mime_types )
	{
		if ( utils::i_starts_with(mt, entry) )
			return true;
	}
	return false;
}

void configuration_wrapper::parse_mime_types(const std::string& str, std::vector<std::string>& types )
{
	const char delimiter{' '};
	auto begin = str.cbegin();
	auto end = str.cbegin();

	//Trim
	size_t offset = str.find_first_not_of(delimiter);
	if( offset != std::string::npos)
		begin += offset;

	offset = str.find_last_not_of(delimiter);
	if( offset != std::string::npos)
		end += offset;

	//Parse
	for( auto it = begin ; it <= end; ++it )
	{
		if( *it == delimiter || it == end )
		{
			types.push_back(std::string{begin, it});
			begin = ++it;
		}
	}

}

void configuration_wrapper::parse_network_file(const string& file, list<network::IPV4Network>& addresses)
{
	fstream in( file, ios_base::in );

	regex re("^allow\\s+(\\d{1,3}).(\\d{1,3}).(\\d{1,3}).(\\d{1,3})(\\/\\d{1,2}){0,1}$");
	string line;

	while ( getline( in, line ) )
	{
		smatch what;
		if ( regex_search ( line, what, re ) )
		{
			const uint8_t o_index = 4;
			const uint8_t offset = 1;
			int octet[o_index];
			for ( int i = 0; i < o_index; ++i )
			{
				octet[i] = stoi ( what[i+offset] );
				if ( octet[i] < 0 || octet[i] > 255 )
					throw std::logic_error("Invalid IPV4 address");
			}

			int bitnum{32};
			if( what[5].matched )
			{
				bitnum = stoi(what[5].str().substr(1));
				if ( bitnum <= 0 || bitnum > 32 )
					throw std::logic_error("Invalid mask");
			}

			string ip;
			for ( int i = 0; i < o_index; ++i )
			{
				ip += to_string( octet[i] );
				if ( i < o_index - 1 )
					ip += ".";
			}

			IPV4Network network( ip, bitnum );
			addresses.push_back( network );
		}
	}
}

void configuration_wrapper::certificate_config::reset() noexcept
{
	certificate_file.clear();
	key_file.clear();
	key_password.clear();
	is_default = false;
}

bool configuration_wrapper::certificate_config::is_complete() const noexcept
{
	return ! ( certificate_file.empty() || key_file.empty() || key_password.empty() );
}


string configuration_wrapper::get_error_filename( uint16_t code ) const
{
	if ( error_filenames.empty() ) throw service::not_initialized{};
	auto it = error_filenames.find( code );
	if ( it == error_filenames.end() )
		return "";
	return it->second;
}
/* currently not used.
static bool is_digit_only ( const string& s )
{
	for(auto& c : s)
		if(!isdigit(c))
			return false;

	return true;
}*/

/*
namespace
{
static bool exists ( const string& what, bool file )
{
	struct stat buf;
	int r = stat( what.c_str(), &buf );
	if ( r < 0 )
		return false;

	if ( file )
		return S_ISREG(buf.st_mode);
	return S_ISDIR(buf.st_mode);
}


static bool dir_exists ( const string& dir )
{
	return exists ( dir, false );
}

static bool file_exists( const string& file )
{
	return exists( file, true );
}
}
*/


certificates_iterator configuration_wrapper::iterator() const
{
	return certificates_iterator{this};
}

void configuration_wrapper::set_port_maybe ( int32_t forced_port )
{
	if ( forced_port > 0 )
		port = forced_port;
	if ( port < 0 && port >= 65536 )
		throw logic_error("port out of range");
}

void configuration_wrapper::set_fd_limit(const size_t fdl) noexcept
{
	//A bunch of FDs are reserved for other usages,
	//therefore limit can't be below that threshold
	if(fdl < reserved_fds())
		fd_limit = reserved_fds();
	else
		fd_limit = fdl;
}

uint16_t configuration_wrapper::get_port() const noexcept
{
	if ( port < 0 || port >= 65535 )
		return default_port;
	return static_cast<uint16_t> ( port );
}

uint16_t configuration_wrapper::get_port_h() const noexcept
{
	if ( porth < 0 || porth >= 65535 )
		return default_port;
	return static_cast<uint16_t> ( porth );
}

certificates_iterator::certificates_iterator( const configuration_wrapper* w ): wrapper{w}, index{0}
{
// 	LOG(INFO) << "Certificate Iterator Instantiated:" << (void*) this;
}

uint64_t configuration_wrapper::get_client_connection_timeout() const noexcept
{
	return client_connection_timeout;
}

uint64_t configuration_wrapper::get_operation_timeout() const noexcept
{
	return operation_timeout;
}

uint64_t configuration_wrapper::get_board_timeout() const noexcept
{
	return board_timeout;
}

const header& configuration_wrapper::header_configuration() const noexcept
{
	return *header_conf;
}

}

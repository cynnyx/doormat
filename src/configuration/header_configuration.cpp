#include "header_configuration.h"

#include <fstream>

namespace configuration 
{

header::header( const std::string& filename )
{
	std::fstream f ( filename, std::ios_base::in );
	init<std::fstream>( f );
}

bool header::filter( const std::string& key ) const noexcept
{
	bool r = to_kill.find( key ) != to_kill.end();
	return r;
}

void header::init_generators()
{
	variable_set t = 
	{
		{ "$msec" },
		{ "$timestamp"},
		{ "$remote_addr" },
		{ "$scheme" },
// 		{ "$request_time" },
		{ "$proxy_add_x_forwarded_for" }
	};
	generators = std::move ( t );
}

const header::variable_set& header::variables() const noexcept
{
	return generators;
}

}

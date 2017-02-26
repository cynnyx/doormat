#include "utils.h"
#include "log_wrapper.h"

#include <boost/regex.hpp>

namespace utils
{

std::string get_first_part( const std::string& s, char delimiter )
{
	std::size_t i = s.find_first_of( delimiter );
	return s.substr(0, i);
}

bool icompare( const std::string& a, const std::string& b )
{
	if (a.length() == b.length())
		return std::equal( b.begin(), b.end(), a.begin(), []( const char& a, const char& b )
		{
			return std::tolower(a) == std::tolower(b);
		} );

	return false;
}

std::string hostname_from_url( const std::string& uri )
{
	boost::regex url_regex ( R"(^(([^:\/?#]+):)?(//([^\/?#]*))?([^?#]*)(\?([^#]*))?(#(.*))?)", boost::regex::extended );

	boost::smatch url_match_result;
	if ( boost::regex_match( uri, url_match_result, url_regex ) )
	{
		int index = 0;
		for ( const auto& res : url_match_result )
		{
			if ( index == 4 )
				return res;
			++index;
		}
	}
	else
		LOGERROR("hostname not valid - ", uri);
	return "";
}

}

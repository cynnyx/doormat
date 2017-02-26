#ifndef HEADER_CONFIGURATION_H
#define HEADER_CONFIGURATION_H

#include <string>
#include <list>
#include <functional>
#include <unordered_map>
#include <type_traits>
#include <unordered_set>

#include "configuration_exception.h"

namespace configuration
{

struct header
{
	using header_generator = std::function<std::string(void)>;
	using header_map = std::unordered_map<std::string, std::string>;
	using header_kill = std::unordered_set<std::string>;
	using variable_set = std::unordered_set<std::string>;
private:
	header_map to_add;
	header_kill to_kill;
	variable_set generators;
	
	void init_generators();
public:
	header( const std::string& config_file );
	header() noexcept {}
	
	// Useful to mock a stream
	template<class Stream>
	void init( Stream& stream )
	{
		init_generators();
		int state{0};
		std::string key;
		std::string value;
		std::string temp;
		
		if ( ! stream.is_open() )
			throw configuration_exception("Config file not open!");
			
		while ( ! stream.eof() )
		{
			stream >> temp;
			if ( temp == "proxy_set_header" )
			{
				if ( state == 0)
					++state;
				else
					throw configuration_exception("Syntax error in set header configuration");
			}
			else if ( state == 1 )
			{
				key = temp;
				++state;
			}
			else if ( state == 2 )
			{
				value = temp;
				++state;
			}
			
			if ( state == 3 )
			{
				if ( key.empty() )
					throw configuration_exception("Empty key in set_header!");
				
				if ( key.find_first_of( '#' ) == std::string::npos ) //Is it a comment?
				{
					if ( value[ value.length() - 1 ] == ';')
						value.pop_back();
					
					if ( value == "$kill" )
						to_kill.insert( key );
					else
					{
						if ( value.find_first_of('$') == 0 )
						{
							if ( generators.find( value ) == generators.end() )
								throw configuration_exception("Unknown variable");
						}
						to_add[ key ] = value;
					}
				}
				value.clear();
				key.clear();
				state = 0;
			}
		}
	}
	bool filter( const std::string& head ) const noexcept;
	bool filter_exist() const noexcept { return to_kill.size() > 0; }
	
	header_map::const_iterator begin() const { return to_add.cbegin(); }
	header_map::const_iterator end() const { return to_add.cend(); }
	
	header_kill::const_iterator begin_kill() const { return to_kill.cbegin(); }
	header_kill::const_iterator end_kill() const { return to_kill.cend(); }
	
	const variable_set& variables() const noexcept;
};

}

#endif // HEADER_CONFIGURATION_H

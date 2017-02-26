#ifndef INIT_H
#define INIT_H

#include <exception>
#include <boost/program_options.hpp>
#include <boost/system/error_code.hpp>

#include <cynnypp/async_fs.hpp>

namespace doormat
{

void set_desc( std::unique_ptr<boost::program_options::options_description> desc );
boost::program_options::options_description& get_desc();

int doormat( int argc, char** argv );

}
#endif // INIT_H

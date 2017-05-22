#include <iostream>
#include <cstring>

#include "cynnypp/async_fs.hpp"

#include "init.h"
#include "utils/log_wrapper.h"
#include "service_locator/service_initializer.h"
#include "network/cloudia_pool.h"

int main( int argc, char* argv[] )
{
	int rv = EXIT_FAILURE;
	try
	{
		service::initializer::set_socket_pool_factory( new network::cloudia_pool_factory );
		
		rv = doormat::doormat( argc, argv );
	}
	catch ( const boost::program_options::required_option& e )
	{
		//should never happen since we don't have compulsory arguments
		std::cerr << e.what() << std::endl;
		std::cout << doormat::get_desc() << std::endl;
	}
	catch ( const boost::program_options::error& e )
	{
		std::cerr << e.what() << std::endl;
		std::cout << doormat::get_desc() << std::endl;
	}
	catch(const cynny::cynnypp::filesystem::ErrorCode& e )
	{
		LOGERROR(e.what());
	}
	catch(const boost::system::error_code& e )
	{
		LOGERROR(e.message());
	}
	catch(int e)
	{
		LOGERROR(strerror(e));
	}
	
	return rv;
}

#pragma once

#include "service_locator.h"
#include "../configuration/configuration_wrapper.h"
#include "../configuration/configuration_parser.h"
#include "../network/cloudia_pool.h"
#include "../io_service_pool.h"
#include "../log/log.h"
#include "../log/inspector_serializer.h"
#include "../stats/stats_manager.h"
#include "../endpoints/chain_factory.h"

#include <thread>

namespace service
{

class initializer
{
public:
	static bool load_configuration(std::string file, bool verbose = false)
	{
		configuration_parser parser{file, verbose};

		auto cw = parser.parse().release();
		if(!cw->get_thread_number())
			cw->set_thread_number(std::thread::hardware_concurrency());
		set_configuration(cw);

		if(verbose && parser.parser_finished_successfully())
		{
	// 		 		std::cout << std::endl << std::endl;
			std::cout << "================================================================================" << std::endl;
			std::cout << "                          CONFIGURATION IS VALID                                " << std::endl;
			std::cout << "================================================================================" << std::endl;
			std::cout << std::endl << std::endl;
		}
		return parser.parser_finished_successfully();
	}

	static void init_services()
	{
		auto& cw = locator::configuration();

		auto sp = new server::io_service_pool{ cw.get_thread_number() };
		set_service_pool(sp);

		auto sm = new stats::stats_manager{ cw.get_thread_number() };
		set_stats_manager(sm);

		auto al = new logging::access_log_c{ cw.get_log_path(), "access"};
		set_access_log(al);
		
		// Is missing socket pool factory of factory
	}

	static void terminate_services()
	{
		locator::_access_log.reset();
		locator::_inspector_log.reset();
		locator::_stats_manager.reset();
		locator::_configuration.reset();
	}

	static void set_configuration(configuration::configuration_wrapper* a)
	{
		locator::_configuration.reset(a);
	}

	static void set_service_pool(server::io_service_pool* a)
	{
		locator::_service_pool.reset(a);
	}

	static void set_access_log(logging::access_log* a)
	{
		locator::_access_log.reset(a);
	}

	static void set_inspector_log(logging::inspector_log* a)
	{
		locator::_inspector_log.reset(a);
	}

	static void set_stats_manager(stats::stats_manager* a)
	{
		locator::_stats_manager.reset(a);
	}
	
	static void set_chain_factory( endpoints::chain_factory* cf )
	{
		locator::chfac.reset( cf );
	}
	
	template<class T = boost::asio::ip::tcp::socket>
	static void set_socket_pool(network::socket_factory<T> *sp)
	{
			locator::_socket_pool<T>.reset(sp);
	}

	template<class T = boost::asio::ip::tcp::socket>
	static void set_socket_pool_factory(network::abstract_factory_of_socket_factory<T>* afosf)
	{
			locator::template _socket_pool_factory<T>.reset( afosf );
	}
};

}//service


#pragma once

#include "service_locator.h"
#include "../fs/fs_manager_wrapper.h"
#include "../configuration/configuration_wrapper.h"
#include "../configuration/configuration_parser.h"
#include "../io_service_pool.h"
#include "../log/log.h"
#include "../log/inspector_serializer.h"
#include "../board/board_map.h"
#include "../stats/stats_manager.h"
#include "../network/socket_factory.h"

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
			std::cout << std::endl << std::endl;
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

		auto fsmw = new fs_manager_wrapper();
		set_fs_manager( fsmw );

		auto sm = new stats::stats_manager{ cw.get_thread_number() };
		set_stats_manager(sm);

		auto dp = new routing::board_map{};
		set_destination_provider(dp);

		auto al = new logging::access_log{ cw.get_log_path(), "access"};
		set_access_log(al);
		
		// Is missing socket pool factory
	}
	
	static void thread_local_socket_pool_initializer()
	{
		auto factory = locator::_socket_pool_factory->get_socket_factory(2);
		set_socket_pool( factory.release() );
	}

	static void terminate_services()
	{
		locator::_access_log.reset();
		locator::_inspector_log.reset();
		locator::_destination_provider.reset();
		locator::_stats_manager.reset();
		locator::_fsm.reset();
		locator::_configuration.reset();
		locator::_socket_pool.reset();
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

	static void set_destination_provider(routing::abstract_destination_provider* a)
	{
		locator::_destination_provider.reset(a);
	}

	static void set_fs_manager(fs_manager_wrapper* a)
	{
		locator::_fsm.reset(a);
	}

	static void set_socket_pool(network::socket_factory *sp)
	{
		locator::_socket_pool.reset(sp);
	}
	
	static void set_socket_pool_factory(network::abstract_factory_of_socket_factory* afosf)
	{
		locator::_socket_pool_factory.reset( afosf );
	}
};

}//service


#include "init.h"

#include <string>
#include <memory>
#include <exception>
#include <boost/program_options.hpp>
#include <boost/system/error_code.hpp>

#include "daemon.h"
#include "constants.h"
#include "signal_handlers.h"
#include "http_server.h"
#include "utils/log_wrapper.h"
#include "service_locator/service_initializer.h"

namespace doormat
{
	
std::unique_ptr<server::http_server> doormat_srv;

std::string configuration_file_path{"/opt/doormat/etc/doormat.config"};
std::string log_level;

bool daemon_mode{false};
bool verbose_mode{false};
bool check_configuration_mode{false};

void terminator()
{
	//Prevent double hit
	static bool once{false};
	if(!once)
	{
		once = true;
		service::locator::service_pool().allow_graceful_termination();
		if(doormat_srv)
			doormat_srv->stop();
	}

	//If you insist...
	_exit(1);
}

static void t_handler(int sig){ terminator(); }
static void l_handler(int sig){ log_wrapper::rotate(); }

std::unique_ptr<boost::program_options::options_description> desc;

void set_desc ( std::unique_ptr<boost::program_options::options_description> desc_ )
{
	desc = std::move( desc_ );
}

boost::program_options::options_description& get_desc() 
{
	namespace po = boost::program_options;
	
	if ( ! desc )
	{
		set_desc( std::unique_ptr<po::options_description>{new po::options_description( "Available options" )} );
	}
	desc->add_options()
		("help, h", "prints the list of available options" )
		( "daemon, d", "let the process become a daemon and gets the working directory from the configuration file ")
		( "config-file,c", po::value<std::string>( &configuration_file_path ), "path to the config file" )
		( "log-level,l", po::value<std::string>( &log_level ),
				"logging level; in increasing order of severity: trace, debug, info, warn, error, fatal" )
		("watchdog,w", po::value<bool>( &watchdog ), "turn on and off our watchdog")
		("verbose,v", "prints logs to standard output")
		("version,r", "prints current version")
		("check-config", "checks if the fetched configuration is ok");
			
	return *desc;
}

static void parse_opt(int argc, char** argv)
{
	namespace po = boost::program_options;

	boost::program_options::options_description& desc = get_desc();

	po::variables_map vm;

// 	try
// 	{
		po::store( po::parse_command_line( argc, argv, desc ), vm );
		//parameter parsing and management
		if ( vm.count( "daemon") )
			daemon_mode = true;

		if ( vm.count( "config-file" ) )
			configuration_file_path = vm["config-file"].as<std::string>();

		if ( vm.count( "log-level" ) )
			log_level = vm["log-level"].as<std::string>();

		if ( vm.count("verbose") )
			verbose_mode = true;

		if( vm.count("check-config") )
			check_configuration_mode = true;

		if ( vm.count( "help" ) )
		{
			std::cout << desc << std::endl;
			exit(EXIT_SUCCESS);
		}
		if ( vm.count("version") )
		{
			std::cout << VERSION;
#ifndef NDEBUG
			std::cout<< " - DEBUG";
#endif
			std::cout<<std::endl;
			exit(EXIT_SUCCESS);
		}
		po::notify( vm );
// 	}
// // 	catch ( const boost::program_options::required_option& e )
// 	{
// 		//should never happen since we don't have compulsory arguments
// 		std::cerr<<e.what()<<std::endl;
// 		std::cout<<desc<<std::endl;
// 		exit(EXIT_FAILURE);
// 	}
// 	catch ( const boost::program_options::error& e )
// 	{
// 		std::cerr<<e.what()<<std::endl;
// 		std::cout<<desc<<std::endl;
// 		exit(EXIT_FAILURE);
// 	}
}

int doormat( int argc, char** argv )
{
	int rv{EXIT_FAILURE};

	parse_opt(argc,argv);

	//Install signals
	signals_handlers::set(SIGTERM, &t_handler);
	signals_handlers::set(SIGINT, &t_handler);
	signals_handlers::set(SIGUSR1, &l_handler);

	//Load Configuration
	bool valid_conf = service::initializer::load_configuration(configuration_file_path, check_configuration_mode);
	if(check_configuration_mode)
		exit(!valid_conf);

// 	try
// 	{
		//Daemonize
		if(daemon_mode)
			becoming_deamon(service::locator::configuration().get_daemon_root());

		//Init logs
		if(log_level.size())
			service::locator::configuration().set_log_level(log_level);
		log_wrapper::init(verbose_mode,
			service::locator::configuration().get_log_level(), service::locator::configuration().get_log_path());

		service::initializer::init_services();

		//Raise file descriptor limit
		auto max_fd = service::locator::configuration().get_fd_limit();
		if( max_fd != 0 )
		{
			rlimit fdl;
			fdl.rlim_cur = max_fd;
			fdl.rlim_max = max_fd;

			if( setrlimit(RLIMIT_NOFILE, &fdl) != 0 )
				LOGERROR("Couldn't change file descriptor limit: ", strerror(errno));
		}
		//Store current value
		else
		{
			rlimit fdl;
			if( getrlimit(RLIMIT_NOFILE, &fdl) != 0 )
				LOGERROR("Couldn't change file descriptor limit: ", strerror(errno));

			service::locator::configuration().set_fd_limit(fdl.rlim_cur);
		}

		doormat_srv.reset(new server::http_server{});

		//Handle user fallback after successfull bind on 443 or 80
		if(getuid() == 0)
		{
			auto usrPtr = getpwnam("doormat");
			if(!usrPtr || setuid ( usrPtr->pw_uid ) < 0 )
				LOGERROR("Setuid failed: ", errno,", continuing as root!");
		}

		//Block signals before spawning threads
		signals_handlers::block_all();

		server::io_service_pool::main_init_fn_t main_init = []()
		{
			//Unblock signals for main thred only
			signals_handlers::unblock_all();
			service::locator::stats_manager().start();
		};


		//Main loop
		doormat_srv->start(main_init);

		//Stopping
		doormat_srv->stop();

		rv = EXIT_SUCCESS;
// 	}
// 	catch(const cynny::cynnypp::filesystem::ErrorCode& e )
// 	{
// 		LOGERROR(e.what());
// 	}
// 	catch(const boost::system::error_code& e )
// 	{
// 		LOGERROR(e.message());
// 	}
// 	catch(int e)
// 	{
// 		LOGERROR(strerror(e));
// 	}

	//Stop services
	service::initializer::terminate_services();

	LOGDEBUG("Bye bye");

	//Stop logs
// 	boost::log::core::get()->remove_all_sinks();

	if(daemon_mode)
		stop_daemon();

	return rv;
}

}

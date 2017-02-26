#include "daemon.h"

#include <iostream>
#include <string>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sched.h>
#include <errno.h>
#include <limits.h>

#ifdef USE_MAP_ANON
#define _BSD_SOURCE             /* Get MAP_ANONYMOUS definition */
#endif
#include <sys/wait.h>
#include <sys/mman.h>
#include <fcntl.h>

#include "utils/log_wrapper.h"

using namespace std;

bool watchdog{false};
static int aborts{0};
static int crashes{0};

static void checkOutcome( pid_t p )
{
	if ( p < 0 )
		throw errno;
}

static bool jail()
{
	const char* v = getenv("DOORMATJAIL");
	return v != nullptr;
}

/**
 * Internal Watchdog
 **/
static void watch( pid_t p )
{
	while ( p != 0 ) // Watchdog cycle
	{
		p = fork();
		checkOutcome(p);

		if ( p != 0 )
		{
			auto log = open ("doormat_watchdog.log", O_WRONLY| O_CREAT| O_APPEND, S_IRWXU|S_IRGRP|S_IROTH); /* stdout */
			if( log >= 0)
			{
				dup2(log, 1);
				close(log);
			}

			int r;
			pid_t dead = waitpid( p, &r, 0);
			if ( dead < 0 )
			{
				std::cerr << strerror(errno);
				std::cerr << "Watchdog error - error in waitpid, this is a  bug. I go away!" << std::endl;
				break;
			}
			else
			{
				if ( WIFEXITED( r ) )
				{
					int exit_status = WEXITSTATUS( r );
					std::cout<<"Watchdog exits"<<std::endl;
					exit( exit_status );
				}
				else if ( WIFSIGNALED ( r ) )
				{
					int term_sig = WTERMSIG( r );

					if ( term_sig == SIGTERM || term_sig == SIGKILL )
					{
						std::cout<<"Watchdog follows its child termination!"<<std::endl;
						exit( term_sig == SIGKILL ? 1 : 0 );
					}
					else if ( term_sig == SIGABRT )
					{
						++aborts;
						std::cerr<<"I am not so happy: I have received n." << aborts << " aborts!"<<std::endl;
					}
					else if ( term_sig == SIGSEGV )
					{
						++crashes;
						std::cerr<<"I am not so happy: I have received n."<< crashes << " sigsegv!"<<std::endl;
					}
#ifdef WCOREDUMP
					if ( WCOREDUMP( r ) )
						std::cerr<<"Watchdog is happy to be useful : at least we have a core file. Good!"<<std::endl;
#endif
					if ( aborts > 10 || crashes > 10 )
					{
						std::cerr <<"Watchdog cries : Doormat died a horrible death!";
						std::cerr <<"Crashes:" << crashes;
						std::cerr <<", Aborts:" << aborts;
						std::cerr << " I run away!" << std::endl;
						exit( aborts + crashes );
					}
				}
			}
		}
	}
}

/**
 * Becoming a daemon, with paranoids error checks!
 **/
static void daemon_mode( const string& root )
{
	pid_t p = fork();
	checkOutcome(p);
	if ( p != 0 )
		exit( EXIT_SUCCESS ); // GrandFather dies!

	p = setsid(); // I am a session leader! How cool!
	checkOutcome(p);

	int d = chdir(root.c_str());
	checkOutcome( (pid_t) d );

	auto pidfile = open("doormat.pid", O_WRONLY| O_CREAT | O_TRUNC, S_IRWXU|S_IRGRP|S_IROTH);
	checkOutcome( (pid_t) pidfile );

	auto pid = std::to_string(p);
	if( write(pidfile, pid.c_str(), pid.size() ) < 0)
		std::cerr<<"Can't write pid file, please check permission."<<std::endl;

	for ( short fd = 0; fd < SHRT_MAX; ++fd )
		close ( fd );

	d = open ("/dev/null", O_RDONLY| O_CREAT |O_APPEND, S_IRWXU|S_IRGRP|S_IROTH); /* stdin */
	checkOutcome( (pid_t) d );

	d = dup (d); /* stdout */
	checkOutcome( (pid_t) d );

	d = dup (d); /* stderror */
	checkOutcome( (pid_t) d );

	if( watchdog )
		watch( p );
}

#ifdef _GNU_SOURCE
void jailify()
{
	if ( jail() )
	{
		int u = unshare ( CLONE_NEWPID | CLONE_NEWUSER | CLONE_NEWNS | CLONE_FS
			| CLONE_FILES | CLONE_SYSVSEM );

		checkOutcome ( (pid_t) u );
	}
}
#endif

/**
 * Becoming a daemon, with paranoids error checks!
 **/
void becoming_deamon(const std::string& root)
{
	if(!root.empty())
	{
		try
		{
			std::cout<<"daemon root is " << root<<std::endl;
			/*
			if ( watchdog )
				std::set_terminate( terminator );
			*/
			daemon_mode( root );
			jailify();
			return;
		}
		catch(int err)
		{
			std::cerr<<"I will not be your daemon, I am a saint and saints are always dead."<<std::endl;
			throw err;
		}
	}

	std::cerr<<"A working directory must be specified if doormat is executed with --daemon."<<std::endl;
	std::cerr<<" [ --- Check the configuration file --- ] "<<std::endl;
	throw EINVAL;
}

void stop_daemon()
{
	remove("doormat.pid");
}


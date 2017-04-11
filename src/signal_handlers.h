#include <pwd.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/resource.h>

namespace signals_handlers
{

inline void set(int sig, __sighandler_t handler)
{
	struct sigaction act0;
	memset (&act0, '\0', sizeof(act0));
	act0.sa_handler = handler;

	if( sigaction(sig, &act0, nullptr ) < 0 )
		throw errno;
}

inline void block_all()
{
	sigset_t mask;
	sigfillset(&mask);
	if( pthread_sigmask(SIG_SETMASK, &mask, NULL) < 0 )
		throw errno;
}

inline void unblock_all()
{
	sigset_t mask;
	sigemptyset(&mask);
	if( pthread_sigmask(SIG_SETMASK, &mask, NULL) < 0 )
		throw errno;
}

}

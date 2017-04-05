#pragma once
#include "../../src/signal_handlers.h"
#include <pwd.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/resource.h>







namespace doormat {
    using ::signals_handlers::set;
    using ::signals_handlers::block_all;
    using ::signals_handlers::unblock_all;
}
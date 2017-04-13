#ifndef DOORMAT_LOGGING_HPP
#define DOORMAT_LOGGING_HPP

/**
 * STL pollutes the global namespace
 * and uses that pollution!
 * So we need them here, because they are in log.h.
 * Holy shit!
 */
#include <string>
#include <memory>

namespace doormat 
{
	
namespace internal
{
#include "../../src/log/log.h"
}

namespace logging 
{
using internal::logging::access_log;
using internal::logging::dummy_al;
}
}

#endif

#ifndef DOORMAT_GENERATOR_H
#define DOORMAT_GENERATOR_H

#include <functional>
#include <memory>
#include "../chain_of_responsibility/node_interface.h"

namespace endpoints
{
	
using generating_function_t = std::function<std::unique_ptr<node_interface>()>;

	
}

#endif

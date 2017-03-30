#ifndef DOORMAT_GENERATOR_H
#define DOORMAT_GENERATOR_H

#include <functional>
#include <memory>

class node_interface;

namespace endpoints
{
	
using generating_function_t = std::function<std::unique_ptr<node_interface>()>;

	
}

#endif

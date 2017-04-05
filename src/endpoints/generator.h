#ifndef DOORMAT_GENERATOR_H
#define DOORMAT_GENERATOR_H

#include <functional>
#include <memory>

class node_interface;

namespace endpoints
{

template<typename, char, bool>
class radix_tree;
	
using chain_generator_t = std::function<std::unique_ptr<node_interface>()>;
using tree_generator_t = std::function<radix_tree<chain_generator_t, '/', false>*()>;

	
}

#endif

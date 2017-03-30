#ifndef DOORMAT_CHAIN_FACTORY_H
#define DOORMAT_CHAIN_FACTORY_H

#include <memory>

#include "generator.h"

namespace http {
class http_request;
}

namespace endpoints 
{

class radix_tree;
	
class chain_factory
{
public:
	using chain_ptr = std::unique_ptr<node_interface>;

	chain_factory(generating_function_t fallback_logic) : fallback_logic{std::move(fallback_logic)} {}
    /** For now we will handle only these three methods.*/
    void get(const std::string &path, generating_function_t logic);
    void post(const std::string &path, generating_function_t logic);
    void put(const std::string &path, generating_function_t logic);

	chain_ptr get_chain(const http::http_request &original_request) const noexcept;

	chain_ptr get_chain_and_params(http::http_request &original_request) const noexcept;

private:
    std::array<std::unique_ptr<radix_tree>, 3> method_prefixes; //contains a different radix_tree for each of the endpoints.
    generating_function_t fallback_logic;
};

}

#endif //DOORMAT_CHAIN_FACTORY_H

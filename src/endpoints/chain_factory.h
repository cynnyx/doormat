#ifndef DOORMAT_CHAIN_FACTORY_H
#define DOORMAT_CHAIN_FACTORY_H

#include "path/radix_tree.h"
#include <memory>

namespace endpoints 
{
	
class chain_factory
{
public:
    chain_factory(generating_function_t fallback_logic) : fallback_logic{std::move(fallback_logic)} {};
    /** For now we will handle only these three methods.*/
    void get(const std::string &path, generating_function_t logic);
    void post(const std::string &path, generating_function_t logic);
    void put(const std::string &path, generating_function_t logic);

    std::unique_ptr<node_interface> get_chain(const http::http_request &original_request) const noexcept;

private:
    std::array<std::unique_ptr<radix_tree>, 3> method_prefixes; //contains a different radix_tree for each of the endpoints.
    generating_function_t fallback_logic;
};

}

#endif //DOORMAT_CHAIN_FACTORY_H

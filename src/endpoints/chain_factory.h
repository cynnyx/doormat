#ifndef DOORMAT_CHAIN_FACTORY_H
#define DOORMAT_CHAIN_FACTORY_H

#include <memory>

#include "generator.h"
#include "../../src/endpoints/path/radix_tree.h"

namespace http {
class http_request;
}

namespace endpoints 
{
	
class chain_factory
{
public:
	using chain_ptr = std::unique_ptr<node_interface>;

	chain_factory(chain_generator_t fallback_logic) noexcept;
	~chain_factory() noexcept;

    /** For now we will handle only these three methods.*/
	void get(const std::string& host, const std::string &path, chain_generator_t logic);
	void post(const std::string& host, const std::string &path, chain_generator_t logic);
	void put(const std::string& host, const std::string &path, chain_generator_t logic);

	chain_ptr get_chain(const http::http_request &original_request) const noexcept;
	chain_ptr get_chain_and_params(http::http_request &original_request) const noexcept;

private:
	enum method { GET, POST, PUT, ALL_BEFORE_THIS };

	void addPattern(method m, const std::string& host, const std::string& path, chain_generator_t logic);

	std::array<std::unique_ptr<radix_tree<tree_generator_t>>, ALL_BEFORE_THIS> host_trees; //contains a different radix_tree for each of the endpoints.
	chain_generator_t fallback_logic;
};

}

#endif //DOORMAT_CHAIN_FACTORY_H

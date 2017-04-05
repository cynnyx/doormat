#include "chain_factory.h"
#include "path/radix_tree.h"
#include "../chain_of_responsibility/node_interface.h"

#include <memory>

namespace endpoints 
{


class chain_maker {
	radix_tree<chain_generator_t, '/', false> t_;

public:
	chain_maker(radix_tree<chain_generator_t, '/', false> tree) noexcept
		: t_{std::move(tree)}
	{}
	radix_tree<chain_generator_t, '/', false>* operator()() noexcept { return &t_; }
};


chain_factory::chain_factory(chain_generator_t fallback_logic) noexcept
	: fallback_logic{std::move(fallback_logic)}
{}

chain_factory::~chain_factory() noexcept
{}

void chain_factory::get(const std::string& host, const std::string &path, chain_generator_t logic)
{
	addPattern(GET, host, path, std::move(logic));
}


void chain_factory::post(const std::string& host, const std::string &path, chain_generator_t logic)
{
	addPattern(POST, host, path, std::move(logic));
}

void chain_factory::put(const std::string& host, const std::string &path, chain_generator_t logic)
{
	addPattern(PUT, host, path, std::move(logic));
}

chain_factory::chain_ptr chain_factory::get_chain(const http::http_request &original_request) const
{
	return get_chain_private(original_request);
}

chain_factory::chain_ptr chain_factory::get_chain_and_params(http::http_request &original_request) const
{
	return get_chain_private<true>(original_request);
}

void chain_factory::addPattern(chain_factory::method m, const std::string& host, const std::string& path, chain_generator_t logic)
{
	if ( ! host_trees[m] )
		host_trees[m] = std::make_unique<radix_tree<tree_generator_t, '.', true>>();

	radix_tree<chain_generator_t, '/', false> tree;
	tree.addPattern(path, std::move(logic));
	chain_maker maker{std::move(tree)};

	host_trees[m]->addPattern(host.empty() ? "*" : host, std::move(maker));
}


}

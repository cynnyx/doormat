#include "chain_factory.h"
#include "path/radix_tree.h"
#include "../chain_of_responsibility/node_interface.h"

#include <memory>

namespace endpoints 
{

//#define GET_BUCKET 0
//#define POST_BUCKET 1
//#define PUT_BUCKET 2


class chain_maker {
	radix_tree<chain_generator_t> t_;

public:
	chain_maker(radix_tree<chain_generator_t> tree) : t_{std::move(tree)} {}
	radix_tree<chain_generator_t>* operator()() { return &t_; }
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

chain_factory::chain_ptr chain_factory::get_chain(const http::http_request &original_request) const noexcept
{
	int bucket;
	switch (original_request.method_code())
	{
		case http_method::HTTP_GET: bucket = GET; break;
		case http_method::HTTP_POST: bucket = POST; break;
		case http_method::HTTP_PUT: bucket = PUT; break;
		default: return nullptr;
	}

	if(host_trees[bucket] == nullptr)
		return fallback_logic();

	auto path_tree = host_trees[bucket]->get(std::string(original_request.hostname()));
	if(!path_tree)
		return fallback_logic();

	auto selected = path_tree->get(original_request.path());

	if(!selected) return fallback_logic();
	return selected;
}

chain_factory::chain_ptr chain_factory::get_chain_and_params(http::http_request &original_request) const noexcept
{
	int bucket;
	switch (original_request.method_code())
	{
		case http_method::HTTP_GET: bucket = GET; break;
		case http_method::HTTP_POST: bucket = POST; break;
		case http_method::HTTP_PUT: bucket = PUT; break;
		default: return nullptr;
	}

	if(host_trees[bucket] == nullptr)
		return fallback_logic();

	auto path_tree = host_trees[bucket]->get(std::string(original_request.hostname()), &original_request);
	if(!path_tree)
		return fallback_logic();

	auto selected = path_tree->get(original_request.path());

	if(!selected) return fallback_logic();
	return selected;
}

void chain_factory::addPattern(chain_factory::method m, const std::string& host, const std::string& path, chain_generator_t logic)
{
	if ( ! host_trees[m] )
		host_trees[m] = std::make_unique<radix_tree<tree_generator_t>>("", '.');

	radix_tree<chain_generator_t> tree{""};
	tree.addPattern(path, std::move(logic));
	chain_maker maker{std::move(tree)};

	host_trees[m]->addPattern(host, std::move(maker));
}


}

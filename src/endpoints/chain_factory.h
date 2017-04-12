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

namespace details {

template<bool with_params, typename R>
struct get_helper;

template<typename R>
struct get_helper<false, R>
{
	using tree_ptr = radix_tree<chain_generator_t, '/', false>*;

	static auto get(tree_ptr t, R&& req)
	{
		return t->get(req.path());
	}
};

template<typename R>
struct get_helper<true, R>
{
	using tree_ptr = radix_tree<chain_generator_t, '/', false>*;

	static auto get(tree_ptr t, R&& req)
	{
		return t->get(req.path(), &req);
	}
};

}
	
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

	chain_ptr get_chain(const http::http_request &original_request) const;
	chain_ptr get_chain_and_params(http::http_request &original_request) const;

private:
	enum method { GET, POST, PUT, ALL_BEFORE_THIS };

	void addPattern(method m, const std::string& host, const std::string& path, chain_generator_t logic);

	template<bool with_params = false, typename R>
	chain_ptr get_chain_private(R&& req) const
	{
		int bucket;
		switch (req.method_code())
		{
			case http_method::HTTP_GET: bucket = GET; break;
			case http_method::HTTP_POST: bucket = POST; break;
			case http_method::HTTP_PUT: bucket = PUT; break;
			default: return nullptr;
		}

		if(host_trees[bucket] == nullptr)
			return fallback_logic();

		const auto& host = req.hostname();
		auto path_tree = host_trees[bucket]->get(host.empty() ? "*" : host);
		if(!path_tree)
			return fallback_logic();
		auto selected = details::get_helper<with_params, R>::get(path_tree, req);
		if(!selected)
			return fallback_logic();

		return selected;
	}

	std::array<std::unique_ptr<radix_tree<tree_generator_t, '.', true>>, ALL_BEFORE_THIS> host_trees; //contains a different radix_tree for each of the endpoints.
	chain_generator_t fallback_logic;
};

}

#endif //DOORMAT_CHAIN_FACTORY_H

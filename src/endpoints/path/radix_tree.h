#ifndef DOORMAT_TREE_H
#define DOORMAT_TREE_H

#include <memory>
#include <functional>
#include <vector>
#include <experimental/optional>

#include "../generator.h"

namespace http 
{
	class http_request;
}

namespace endpoints 
{


class radix_tree
{
public:
	radix_tree(std::string label, char splitToken='/'): node_label{std::move(label)}, splitToken{splitToken}
	{}

	void addPattern(const std::string &path_pattern, generating_function_t gen);
	bool matches(const std::string &path, http::http_request*r=nullptr) const;
	std::unique_ptr<node_interface> get(const std::string& str, http::http_request*r = nullptr) const;
private:
	using str_it = std::string::const_iterator;
	using vec_it = std::vector<std::string>::iterator;
	using optional_tree = std::experimental::optional<const radix_tree*>;

	optional_tree matches(str_it path_it, str_it end, http::http_request*r=nullptr) const;

	optional_tree wildcard_matches(str_it path_it, str_it end, http::http_request*r=nullptr) const;

	optional_tree parameter_matches(str_it path_it, str_it end, http::http_request*r=nullptr) const;

	void addChild(vec_it begin, vec_it cend, generating_function_t gen);

	void appendChild(vec_it begin, vec_it end, generating_function_t gen);

	const std::string node_label;
	std::experimental::optional<generating_function_t> generating_function;
	std::vector<std::unique_ptr<radix_tree>> childs;
	const char splitToken;
};

}

#endif //DOORMAT_TREE_H

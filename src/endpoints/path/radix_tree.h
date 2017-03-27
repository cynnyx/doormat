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
	{};

	void addPattern(const std::string &path_pattern, generating_function_t gen);
	bool matches(const std::string &path, http::http_request*r=nullptr) const;
	std::unique_ptr<node_interface> get(const std::string& str, http::http_request*r = nullptr) const;
private:
	std::experimental::optional<const radix_tree*> matches(std::string::const_iterator path_it, std::string::const_iterator end, http::http_request*r=nullptr) const;

	std::experimental::optional<const radix_tree*> wildcard_matches(std::string::const_iterator path_it, std::string::const_iterator end, http::http_request*r=nullptr) const;

	std::experimental::optional<const radix_tree*> parameter_matches(std::string::const_iterator path_it, std::string::const_iterator end, http::http_request*r=nullptr) const;

	void addChild(std::vector<std::string>::iterator begin, std::vector<std::string>::iterator cend, generating_function_t gen);

	void appendChild(std::vector<std::string>::iterator begin, std::vector<std::string>::iterator end, generating_function_t gen);

	const std::string node_label;
	std::experimental::optional<generating_function_t> generating_function;
	std::vector<std::unique_ptr<radix_tree>> childs;
	const char splitToken;
};

}

#endif //DOORMAT_TREE_H

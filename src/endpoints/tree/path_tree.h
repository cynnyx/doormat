#ifndef DOORMAT_TREE_H
#define DOORMAT_TREE_H

#include <memory>
#include <functional>
#include <vector>
#include <iostream>
#include <algorithm>
#include <experimental/optional>
#include "../../chain_of_responsibility/node_interface.h"

namespace http {
    class http_request;
}

namespace endpoints {

using generating_function_t = std::function<std::unique_ptr<node_interface>()>;
class path_tree
{
public:
    path_tree(std::string label, char splitToken='/')
    : node_label{std::move(label)}, splitToken{splitToken}
    {};

    void addPattern(const std::string &path_pattern, generating_function_t gen);

    bool matches(const std::string &path) const;

    std::unique_ptr<node_interface> get(const std::string& str) const;

private:

    std::experimental::optional<const path_tree*> matches(std::string::const_iterator path_it, std::string::const_iterator end) const;

    std::experimental::optional<const path_tree*> wildcard_matches(std::string::const_iterator path_it, std::string::const_iterator end) const;

    std::experimental::optional<const path_tree*> parameter_matches(std::string::const_iterator path_it, std::string::const_iterator end) const;

    void addChild(std::vector<std::string>::iterator begin, std::vector<std::string>::iterator cend, generating_function_t gen);

    void appendChild(std::vector<std::string>::iterator begin, std::vector<std::string>::iterator end, generating_function_t gen);
    

    const std::string node_label;
    std::experimental::optional<generating_function_t> generating_function;
    std::vector<std::unique_ptr<path_tree>> childs;
    const char splitToken;

};

}

#endif //DOORMAT_TREE_H

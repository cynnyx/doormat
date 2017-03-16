#include "tree.h"
#include <sstream>
#include <string>

namespace endpoints {

generating_function_t default_function = [](){ return std::unique_ptr<node_interface>{nullptr};};

void tree::addPattern(const std::string &path_pattern, generating_function_t gen)
{
    if(!path_pattern.size()) throw std::invalid_argument{"Cannot insert empty pattern in tree."};
    if(node_label == "/" && path_pattern == "/") { //special case: root
        generating_function = std::experimental::optional<generating_function_t>{gen};
        return;
    }
    std::stringstream ss{path_pattern};
    std::vector<std::string> tokens;
    while(ss.good()) {
        std::string tmp;
        std::getline(ss, tmp, splitToken);
        if(tmp.size()) tokens.push_back(std::move(tmp));
    }
    addChild(tokens.begin(), tokens.end(), gen);
}

void tree::addChild(std::vector<std::string>::iterator begin, std::vector<std::string>::iterator end, generating_function_t gen) {
    /** - We already match with the previous by inductive hypothesis. So we just have to move forward.*/
    if(begin == end)
    {
        if(bool(generating_function)) throw std::invalid_argument{"trying to insert duplicated value in node with label " + node_label};
        std::cout << "no terminal... it is not duplicated!" << std::endl;
        generating_function = std::experimental::optional<generating_function_t>{gen};
        return;
    }
    const auto& curLabel = *begin; //this is the label that should match with my child.
    for(auto &&c : childs) {
        if(c->node_label == curLabel) {
            return c->addChild(begin+1, end, std::move(gen));
        }
    }
    //if we're here, this means that no child matches the current token; hence we will need to add it.
    appendChild(begin, end, std::move(gen));
}
//we're sure that begin != end as appendChild is called by addPattern, which checks it in advance.
void tree::appendChild(std::vector<std::string>::iterator begin, std::vector<std::string>::iterator end, generating_function_t gen) {
    childs.push_back(std::make_unique<tree>(*begin, splitToken));
    ++begin;
    if(begin != end) {
        childs.back()->addChild(begin, end, std::move(gen));
    } else {
        std::cout << "assigning generating function!" << std::endl;
        childs.back()->generating_function = std::experimental::optional<generating_function_t>{gen};
    }
}

bool tree::matches(const std::string &path) const
{   if(node_label == "*") {
        return wildcard_matches(path.cbegin(), path.cend());
    }
    if(node_label[0] == '{' && node_label[node_label.size()-1] == '}') {
        return parameter_matches(path.cbegin(), path.cend());
    }

    //word by word matching
    auto label_iterator = node_label.begin();
    auto path_iterator = path.begin();
    while(label_iterator != node_label.end() && path_iterator != path.end() && *path_iterator == *label_iterator) {
        ++path_iterator;
        ++label_iterator;
    }

    if(label_iterator == node_label.end()) {
        //it matches, but only with a partial path. so it is as if it does not match :D
        if(path_iterator == path.end()) return bool(generating_function);
        /* Path has not been consumed: demand it to the childs. */
        for(auto &&c : childs) {
            if(c->matches(path_iterator, path.end())) return true;
        }

        return false;
    }
}


bool tree::wildcard_matches(std::string::const_iterator path_it, std::string::const_iterator end) const
{
    auto nextPathBegin = std::find(path_it, end, '/');
    if(nextPathBegin == end) return bool(generating_function);

    for(auto &&c: childs) {
        if(c->matches(nextPathBegin, end)) return true;
    }
    //if none of the child matches, we match only if * is an available termination.
    return bool(generating_function);
}


bool tree::parameter_matches(std::string::const_iterator path_it, std::string::const_iterator end) const
{
    auto nextPathBegin = std::find(path_it, end, '/');
    if(nextPathBegin == end) return bool(generating_function); //we matched with a parameter.
    for(auto &&c: childs) {
        if(c->matches(nextPathBegin, end)) return true;
    }
    return false;
}



}
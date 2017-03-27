#include "radix_tree.h"
#include <sstream>
#include <string>
#include <algorithm>

namespace endpoints 
{

generating_function_t default_function = [](){ return std::unique_ptr<node_interface>{nullptr};};

void radix_tree::addPattern(const std::string &path_pattern, generating_function_t gen)
{
	if(!path_pattern.size()) 
		throw std::invalid_argument{"Cannot insert empty pattern in tree."};
	if(node_label == "/" && path_pattern == "/") 
	{
		//special case: root
		generating_function = std::experimental::optional<generating_function_t>{gen};
		return;
	}
	std::stringstream ss{path_pattern};
	std::vector<std::string> tokens;
	while(ss.good()) 
	{
		std::string tmp;
		std::getline(ss, tmp, splitToken);
		if(tmp.size()) tokens.push_back(std::move(tmp));
	}
	addChild(tokens.begin(), tokens.end(), gen);
}

void radix_tree::addChild(std::vector<std::string>::iterator begin, 
	std::vector<std::string>::iterator end, generating_function_t gen) 
{
	/** - We already match with the previous by inductive hypothesis. So we just have to move forward.*/
	if(begin == end)
	{
		if(bool(generating_function)) 
			throw std::invalid_argument{"trying to insert duplicated value in node with label " + node_label};
		generating_function = std::experimental::optional<generating_function_t>{gen};
		return;
	}
	const auto& curLabel = *begin; //this is the label that should match with my child.
	for(auto &&c : childs) 
	{
		if(c->node_label == curLabel)
			return c->addChild(begin+1, end, std::move(gen));
	}
	//if we're here, this means that no child matches the current token; hence we will need to add it.
	appendChild(begin, end, std::move(gen));
}

//we're sure that begin != end as appendChild is called by addPattern, which checks it in advance.
void radix_tree::appendChild(std::vector<std::string>::iterator begin, std::vector<std::string>::iterator end, generating_function_t gen) 
{
	childs.push_back(std::make_unique<radix_tree>(*begin, splitToken));
	++begin;
	if(begin != end)
		childs.back()->addChild(begin, end, std::move(gen));
	else
		childs.back()->generating_function = std::experimental::optional<generating_function_t>{gen};
}

bool radix_tree::matches(const std::string &path, http::http_request*r) const
{   
	if(!path.size()) return true;
	return bool(matches(path.cbegin(), path.cend(), r));
}

std::experimental::optional<const radix_tree*> radix_tree::matches(std::string::const_iterator path_it, 
	std::string::const_iterator end, http::http_request*r) const
{
	while(path_it !=  end && *path_it== splitToken) ++path_it;
	if(node_label == "*")
		return wildcard_matches(path_it, end, r);

	if(node_label[0] == '{' && node_label[node_label.size()-1] == '}')
		return parameter_matches(path_it, end, r);

	//word by word matching
	auto label_iterator = node_label.begin();
	auto path_iterator = path_it;
	while(label_iterator != node_label.end() && path_iterator != end && *path_iterator == *label_iterator) 
	{
		++path_iterator; ++label_iterator;
	}

	if(label_iterator == node_label.end()) 
	{
		//it matches, but only with a partial path. so it is as if it does not match :D
		if(path_iterator == end)
			return bool(generating_function) ? this : std::experimental::optional<const radix_tree*>{};
		/* Path has not been consumed: demand it to the childs. */
		
		for(auto &&c : childs) 
		{
			auto tptr = c->matches(path_iterator, end, r);
			if(bool(tptr)) return tptr;
		}
	}

	return std::experimental::optional<const radix_tree*>{};
}

std::experimental::optional<const radix_tree*> radix_tree::wildcard_matches(std::string::const_iterator path_it, std::string::const_iterator end, http::http_request*r) const
{
	auto nextPathBegin = std::find(path_it, end, splitToken);
	if ( nextPathBegin == end ) 
		return bool(generating_function) ? this : std::experimental::optional<const radix_tree*>{};

	for(auto &&c: childs) 
	{
		auto tptr = c->matches(nextPathBegin, end, r);
		if(bool(tptr)) return tptr;
	}
	//if none of the child matches, we match only if * is an available termination.
	return bool(generating_function) ? this : std::experimental::optional<const radix_tree*>{};
}


std::experimental::optional<const radix_tree*> radix_tree::parameter_matches(std::string::const_iterator path_it, std::string::const_iterator end, http::http_request*r) const
{
	auto nextPathBegin = std::find(path_it, end, splitToken);
	if(nextPathBegin == end) {
		if(bool(generating_function)) {
            if(r) r->addParameter(std::string{this->node_label.begin()+1, this->node_label.end()-1}, std::string(path_it, end));
            return this;
        } return std::experimental::optional<const radix_tree*>{}; //we matched with a parameter.
    }
	for(auto &&c: childs) 
	{
		auto tptr = c->matches(nextPathBegin, end, r);
		if(bool(tptr)) {
            if(r) r->addParameter(std::string{this->node_label.begin()+1, this->node_label.begin()-1}, std::string{path_it, nextPathBegin});
            return tptr;
        }
	}
	return std::experimental::optional<const radix_tree*>{};
}

std::unique_ptr<node_interface> radix_tree::get(const std::string &str, http::http_request *r) const
{
	if(str.empty()) return nullptr;
	auto treeptr = matches(str.cbegin(), str.cend(), r);
	if(bool(treeptr))
		return treeptr.value()->generating_function.value()();
	return nullptr;
}



}

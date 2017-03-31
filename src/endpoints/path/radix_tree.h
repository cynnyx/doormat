#ifndef DOORMAT_TREE_H
#define DOORMAT_TREE_H

#include <memory>
#include <functional>
#include <vector>
#include <experimental/optional>
#include <sstream>
#include <string>
#include <algorithm>

#include "../generator.h"
#include "../../chain_of_responsibility/node_interface.h"

namespace http 
{
	class http_request;
}

namespace endpoints 
{


template<typename G = generating_function_t>
class radix_tree
{
public:
	radix_tree(std::string label, char splitToken='/'): node_label{std::move(label)}, splitToken{splitToken}
	{}

	void addPattern(const std::string &path_pattern, G gen)
	{
		if(!path_pattern.size())
			throw std::invalid_argument{"Cannot insert empty pattern in tree."};
		if(node_label == "/" && path_pattern == "/")
		{
			//special case: root
			generating_function = std::experimental::optional<G>{gen};
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
	bool matches(const std::string &path, http::http_request*r=nullptr) const
	{
		if(!path.size()) return true;
		return bool(matches(path.cbegin(), path.cend(), r));
	}
	std::unique_ptr<node_interface> get(const std::string& str, http::http_request*r = nullptr) const
	{
		if(str.empty()) return nullptr;
		auto treeptr = matches(str.cbegin(), str.cend(), r);
		if(bool(treeptr))
			return treeptr.value()->generating_function.value()();
		return nullptr;
	}
private:
	using str_it = std::string::const_iterator;
	using vec_it = std::vector<std::string>::iterator;
	using optional_tree = std::experimental::optional<const radix_tree<G>*>;

	optional_tree matches(str_it path_it, str_it end, http::http_request*r=nullptr) const
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
				return bool(generating_function) ? this : radix_tree::optional_tree{};
			/* Path has not been consumed: demand it to the childs. */

			for(auto &&c : childs)
			{
				auto tptr = c->matches(path_iterator, end, r);
				if(bool(tptr)) return tptr;
			}
		}

		return radix_tree::optional_tree{};
	}

	optional_tree wildcard_matches(str_it path_it, str_it end, http::http_request*r=nullptr) const
	{
		auto nextPathBegin = std::find(path_it, end, splitToken);
		if ( nextPathBegin == end )
			return bool(generating_function) ? this : radix_tree::optional_tree{};

		for(auto &&c: childs)
		{
			auto tptr = c->matches(nextPathBegin, end, r);
			if(bool(tptr)) return tptr;
		}
		//if none of the child matches, we match only if * is an available termination.
		return bool(generating_function) ? this : radix_tree::optional_tree{};
	}

	optional_tree parameter_matches(str_it path_it, str_it end, http::http_request*r=nullptr) const
	{
		auto nextPathBegin = std::find(path_it, end, splitToken);
		if(nextPathBegin == end) {
			if(bool(generating_function)) {
				if(r) r->addParameter(std::string{this->node_label.begin()+1, this->node_label.end()-1}, std::string(path_it, end));
				return this;
			} return radix_tree::optional_tree{}; //we matched with a parameter.
		}
		for(auto &&c: childs)
		{
			auto tptr = c->matches(nextPathBegin, end, r);
			if(bool(tptr)) {
				if(r) r->addParameter(std::string{this->node_label.begin()+1, this->node_label.begin()-1}, std::string{path_it, nextPathBegin});
				return tptr;
			}
		}
		return radix_tree::optional_tree{};
	}

	void addChild(vec_it begin, vec_it end, G gen)
	{
		/** - We already match with the previous by inductive hypothesis. So we just have to move forward.*/
		if(begin == end)
		{
			if(bool(generating_function))
				throw std::invalid_argument{"trying to insert duplicated value in node with label " + node_label};
			generating_function = std::experimental::optional<G>{gen};
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

	void appendChild(vec_it begin, vec_it end, G gen)
	{
		childs.push_back(std::make_unique<radix_tree<G>>(*begin, splitToken));
		++begin;
		if(begin != end)
			childs.back()->addChild(begin, end, std::move(gen));
		else
			childs.back()->generating_function = std::experimental::optional<G>{gen};
	}

	const std::string node_label;
	std::experimental::optional<G> generating_function;
	std::vector<std::unique_ptr<radix_tree<G>>> childs;
	const char splitToken;
};

}

#endif //DOORMAT_TREE_H

#ifndef DOORMAT_TREE_H
#define DOORMAT_TREE_H

#include <memory>
#include <functional>
#include <vector>
#include <experimental/optional>
#include <sstream>
#include <string>
#include <algorithm>
#include <type_traits>

#include "../../chain_of_responsibility/node_interface.h"

namespace http 
{
	class http_request;
}

namespace endpoints 
{

namespace details {

template<bool>
struct direction_helper;

template<>
struct direction_helper<false> {
	template<typename C>
	static auto cbegin(const C& container)
	{
		return std::cbegin(container);
	}

	template<typename C>
	static auto cend(const C& container)
	{
		return std::cend(container);
	}
};

template<>
struct direction_helper<true> {
	template<typename C>
	static auto cbegin(const C& container)
	{
		return std::crbegin(container);
	}

	template<typename C>
	static auto cend(const C& container)
	{
		return std::crend(container);
	}
};

inline
std::string from_iterators(
		std::string::const_iterator beg,
		std::string::const_iterator end
		)
{
	return std::string{beg, end};
}

inline
std::string from_iterators(
		std::string::const_reverse_iterator beg,
		std::string::const_reverse_iterator end
		)
{
	return std::string{end.base(), beg.base()};
}

} // namespace details

template<typename G, char S, bool R>
class radix_tree
{
	radix_tree(std::string label)
		: node_label{std::move(label)}
	{}

public:
	using direction = details::direction_helper<R>;
	using generator_t = G;
	static constexpr auto split_token = S;

	radix_tree() = default;

	radix_tree(const radix_tree& t)
		: node_label{t.node_label}
		, generating_function{t.generating_function}
	{
		for(auto& ptr : t.childs)
			childs.emplace_back(std::make_unique<radix_tree<generator_t, split_token, R>>(*ptr));
	}

	radix_tree(radix_tree&&) = default;

	void addPattern(const std::string &path_pattern, generator_t gen)
	{
		if(!path_pattern.size())
			throw std::invalid_argument{"Cannot insert empty pattern in tree."};

		if(node_label == "/" && path_pattern == "/") // TODO: this seems pretty path-specific...
		{
			//special case: root
			generating_function = std::experimental::optional<generator_t>{gen};
			return;
		}

		std::stringstream ss{path_pattern};
		std::vector<std::string> tokens;
		while(ss.good())
		{
			std::string tmp;
			std::getline(ss, tmp, split_token);
			if(tmp.size()) tokens.push_back(std::move(tmp));
		}

		addChild(direction::cbegin(tokens), direction::cend(tokens), gen);
	}

	bool matches(const std::string &path, http::http_request*r=nullptr) const
	{
		if(!path.size()) return true;
		return bool(matches(direction::cbegin(path), direction::cend(path), r));
	}

	typename generator_t::result_type get(const std::string& str, http::http_request*r = nullptr) const
	{
		if(str.empty()) return {};
		auto treeptr = matches(direction::cbegin(str), direction::cend(str), r);
		if(bool(treeptr))
			return treeptr.value()->generating_function.value()();
		return {};
	}
private:
	using optional_tree = std::experimental::optional<const radix_tree<generator_t, split_token, R>*>;

	template<typename str_it>
	optional_tree matches(str_it beg, str_it end, http::http_request*r=nullptr) const
	{
		static_assert(std::is_same<str_it, std::string::const_iterator>::value
					  || std::is_same<str_it, std::string::const_reverse_iterator>::value,
					  "wrong type");

		while(beg != end && *beg == split_token) ++beg;

		if(node_label == "*")
			return wildcard_matches(beg, end, r);

		if(node_label[0] == '{' && node_label[node_label.size()-1] == '}')
			return parameter_matches(beg, end, r);

		//word by word matching
		auto label_iterator = direction::cbegin(node_label);
		auto path_iterator = beg;
		while(label_iterator != direction::cend(node_label) && path_iterator != end && *path_iterator == *label_iterator)
		{
			++path_iterator; ++label_iterator;
		}

		if(label_iterator == direction::cend(node_label))
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

	template<typename str_it>
	optional_tree wildcard_matches(str_it beg, str_it end, http::http_request*r=nullptr) const
	{
		static_assert(std::is_same<str_it, std::string::const_iterator>::value
					  || std::is_same<str_it, std::string::const_reverse_iterator>::value,
					  "wrong type");

		auto nextPathBegin = std::find(beg, end, split_token);
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

	template<typename str_it>
	optional_tree parameter_matches(str_it path_it, str_it end, http::http_request*r=nullptr) const
	{
		static_assert(std::is_same<str_it, std::string::const_iterator>::value
					  || std::is_same<str_it, std::string::const_reverse_iterator>::value,
					  "wrong type");

		auto nextPathBegin = std::find(path_it, end, split_token);
		if(nextPathBegin == end) {
			if(bool(generating_function)) {
                if(r) r->setParameter(std::string{node_label.begin()+1, node_label.end()-1}, details::from_iterators(path_it, end));
				return this;
			} return radix_tree::optional_tree{}; //we matched with a parameter.
		}
		for(auto &&c: childs)
		{
			auto tptr = c->matches(nextPathBegin, end, r);
			if(bool(tptr)) {
                if(r) r->setParameter(std::string{node_label.begin()+1, node_label.end()-1}, details::from_iterators(path_it, nextPathBegin));
				return tptr;
			}
		}
		return radix_tree::optional_tree{};
	}

	template<typename vec_it>
	void addChild(vec_it begin, vec_it end, generator_t gen)
	{
		static_assert(std::is_same<vec_it, std::vector<std::string>::const_iterator>::value
					  || std::is_same<vec_it, std::vector<std::string>::const_reverse_iterator>::value,
					  "wrong type");

		/** - We already match with the previous by inductive hypothesis. So we just have to move forward.*/
		if(begin == end)
		{
			if(bool(generating_function))
				throw std::invalid_argument{"trying to insert duplicated value in node with label " + node_label};
			generating_function = std::experimental::optional<generator_t>{gen};
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

	template<typename vec_it>
	void appendChild(vec_it begin, vec_it end, generator_t gen)
	{
		static_assert(std::is_same<vec_it, std::vector<std::string>::const_iterator>::value
					  || std::is_same<vec_it, std::vector<std::string>::const_reverse_iterator>::value,
					  "wrong type");

		// helper that enable using std::make_unique with private ctors
		struct enable_make : radix_tree<generator_t, split_token, R> {
			enable_make(std::string label)
				: radix_tree<generator_t, split_token, R>{std::move(label)}
			{}
		};

		childs.push_back(std::make_unique<enable_make>(*begin));
		++begin;
		if(begin != end)
			childs.back()->addChild(begin, end, std::move(gen));
		else
			childs.back()->generating_function = std::experimental::optional<generator_t>{std::move(gen)};
	}

	const std::string node_label;
	std::experimental::optional<generator_t> generating_function;
	std::vector<std::unique_ptr<radix_tree<generator_t, split_token, R>>> childs;
};

template<typename G, char S, bool R>
constexpr char radix_tree<G, S, R>::split_token;

}

#endif //DOORMAT_TREE_H

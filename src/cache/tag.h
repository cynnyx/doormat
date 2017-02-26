#ifndef DOORMAT_TAG_H
#define DOORMAT_TAG_H

#include <vector>
#include <functional>
#include <algorithm>
#include <utility>

template<typename tag_t, typename key_t, unsigned int set_expected_size = 64>
class tagger
{
public:
	inline void insert(const tag_t &tag, const key_t &k)
	{
		for(auto &v: tag_manager)
		{
			if(v.first == tag)
			{
				insert(v.second, k);
				return;
			}
		}

		//tag does not exist.
		auto new_tag =std::make_pair(tag, std::vector<key_t>());
		insert(new_tag.second, k);
		tag_manager.push_back(std::move(new_tag));
		return;
	}

	inline void insert(std::vector<key_t> &where, const key_t &k)
	{
		where.push_back(k);
	}

	inline tag_t remove(const key_t &k)
	{
		for(auto &v: tag_manager)
		{
			auto b = std::find(v.second.begin(), v.second.end(), k);
			if(b != v.second.end())
			{
				v.second.erase(b);
				return v.first;
			}
		}

		return tag_t{};
	}

	inline void clear(const tag_t &tag, std::function<void(key_t)> &&callback)
	{
		for(auto v = tag_manager.begin(); v != tag_manager.end(); ++v)
		{
			if(v->first == tag)
			{
				clear(*v, std::move(callback));
				tag_manager.erase(v);
				return;
			}
		}
	}

private:

	inline void remove(std::vector<key_t> &what, const key_t &k)
	{
		auto b = std::find(what.begin(), what.end(), k);
		what.erase(b);
	}

	inline void remove(const tag_t &tag, const key_t &k)
	{
		for(auto &v: tag_manager)
		{
			if (v.first == tag)
			{
				remove(v.second, k);
				return;
			}
		}
	}

	inline void clear(const std::pair<tag_t, std::vector<key_t>> &what, std::function<void(key_t)> && callback)
	{
		for(auto &v: what.second)
		{
			callback(v);
		}
	}

	std::vector<std::pair<tag_t, std::vector<key_t>>> tag_manager;
};

#endif //DOORMAT_TAG_H

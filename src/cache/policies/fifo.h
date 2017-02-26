#ifndef DOORMAT_FIFO_H
#define DOORMAT_FIFO_H

#include <functional>
#include <list>
#include <utility>

template<typename key>
class fifo_policy
{
public:

	inline void set_remove_callback(std::function<void(key)> &&remove_function)
	{
		this->remove_function = std::move(remove_function);
	}

	inline void touch(const key& k)
	{

	}

	inline void put(const key& k, size_t size)
	{
		fifo.push_back(std::make_pair(k, size));
	}

	inline size_t free_space(const size_t size_to_free)
	{
		auto total_size = 0;
		while (total_size < size_to_free && fifo.size())
		{
			auto element_to_remove = fifo.front().first;
			remove_function(element_to_remove);
			total_size += fifo.front().second;
			fifo.pop_front();
		}
		return total_size;
	}

	inline void remove(key k)
	{
		for(auto &el: fifo)
		{
			if(el.first == k)
			{
				fifo.remove(el);
				return;
			}
		}
	}

private:
	std::list<std::pair<const key, size_t>> fifo;
	std::function<void(key)> remove_function;
};


#endif //DOORMAT_FIFO_H

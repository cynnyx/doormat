#ifndef DOORMAT_CACHE_INSERTION_BUFFER_H
#define DOORMAT_CACHE_INSERTION_BUFFER_H

#include <unordered_map>
#include <vector>

template<typename insert_key_t, typename value_t, typename hasher, unsigned int initial_size>
class cache_insertion_buffer
{
	size_t insertion_token = 0;
	//this is similar to cuckoo hashing.
public:
	cache_insertion_buffer() : base_hash(), buckets(initial_size)
	{
		buckets.reserve(initial_size);
	}

	template<typename...Args>
	int reserve_bucket(const insert_key_t &k, Args&&... args)
	{
		unsigned int elected_position,position;
		elected_position = 0;
		position = get_seed_hash(k);
		bool found = false;
		while(position <  bucket_size)
		{
			if(buckets[position].key == k) return -1; //unauthorized: the key is the same.

			if(buckets[position].key.size() == 0 && !elected_position)
			{
				elected_position = position;
			}
			position += initial_size;
		}

		/* Double the size, but avoid rehashing.*/
		if(elected_position == 0)
		{
			bucket_size *=2 ;
			buckets.resize(bucket_size);
			elected_position = position;
		}
		//at this point the position is for sure a free one; in the former case because we checked, in the latter because we are creating the novel position from scratch.
		buckets[elected_position] = value_t(k, std::forward<Args>(args)...);

		return elected_position;
	}

	void push_data(const int token, const std::vector<void> &data)
	{
		if(buckets[token].key.size())
		{
			buckets[token].data.insert(buckets[token].data.end(), data.begin(), data.end()); //copy temporary data in here.
		}
	}

	template<typename T>
	void push_data(const int token, T begin, T end)
	{
		if(buckets[token].key.size())
		{
			buckets[token].data.insert(buckets[token].data.end(), data.begin(), data.end());
		}
	}

	value_t& get_temporary_element(int token)
	{
		return buckets[token];
	}

	void remove_element(int token)
	{
		clear_element(token); //last one.
	}

private:
	hasher base_hash;
	std::vector<value_t> buckets;
	size_t bucket_size = initial_size;

	void clear_element(int pos)
	{
		buckets[pos].key.clear();
		buckets[pos].data.clear();
		buckets[pos].ttl = 0;
		buckets[pos].tag.clear();
	}

	constexpr unsigned int get_seed_hash(const insert_key_t &k)
	{
		return base_hash(k) % initial_size;
	}
};


#endif //DOORMAT_CACHE_INSERTION_BUFFER_H

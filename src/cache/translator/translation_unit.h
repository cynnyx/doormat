#include <unordered_map>
#include <array>
#include <functional>
#include <chrono>
#ifndef DOORMAT_INTEGER_TRANSLATOR_H
#define DOORMAT_INTEGER_TRANSLATOR_H



template<typename translated_key, typename retrieved_element, typename hasher, int max_number_of_elements = 1024*1024>
class translation_unit{
public:
	//used to communicate on the outside that an element has been deleted.
	using element_removed_callback_t = std::function<void(const translated_key &, const int, std::chrono::system_clock::time_point, size_t)>;
	translation_unit()
	{
		openaddressing_map = new std::array<retrieved_element*, max_number_of_elements>();
	};


	void set_remove_callback(element_removed_callback_t &&erc)
	{
		element_removed_callback = std::move(erc);
	}

	template<typename... Args>
	int insert(const translated_key &k, Args&&... args)
	{
		int position = hasher()(k) % max_number_of_elements;
		int iters = 0;
		while((*openaddressing_map)[position] != nullptr && iters++ < max_number_of_elements)
		{
			auto &element = (*openaddressing_map)[position];
			if(!element->valid()) { remove_with_notification(element->key, element->id); break;}
			position = (position + 1) % max_number_of_elements;
		}
		if(iters != max_number_of_elements)
		{
			retrieved_element *cur_element = new retrieved_element(k, position, std::forward<Args>(args)...);
			(*openaddressing_map)[position] = cur_element;
			outerkey_translator.emplace(k, cur_element);
			return position;
		}
		return -1;
	}


	bool has(const translated_key &key)
	{
		auto element = outerkey_translator.find(key);
		if(element != outerkey_translator.end() && element->second->valid()) return true;
		if(element != outerkey_translator.end() && !element->second->valid())
		{
			remove_with_notification(key, element->second->id);
		}
		return false;
	}

	bool has(const unsigned int translated)
	{
		if((*openaddressing_map)[translated] != nullptr && (*openaddressing_map)[translated]->valid()) return true;
		if((*openaddressing_map)[translated] != nullptr && !(*openaddressing_map)[translated]->valid())
		{
			remove_with_notification((*openaddressing_map)[translated]->key, translated);
		}

		return false;
	}

	const translated_key& get_key(const unsigned int translated) const
	{
		return (*openaddressing_map)[translated]->key;
	}

	const int get_id(const translated_key & key)
	{
		auto element = outerkey_translator.find(key);
		if (element != outerkey_translator.end() && element->second->valid())
		{
			return element->second->id;
		}
		if(!element->second->valid()) remove_with_notification(key, element->second->id);
		return -1;
	}

	const retrieved_element * const  get_element(const translated_key&key)
	{
		auto element = outerkey_translator.find(key);
		if (element != outerkey_translator.end())
		{
            if(element->second != nullptr) 
			{
				if(!element->second->valid())
				{
					remove_with_notification(key, element->second->id);
					return nullptr;
				}
				else 
				{
					return element->second;
				}
			}
			return element->second;
		}

		return nullptr;
	}

	const retrieved_element * const get_element(const unsigned int id)
	{
		if((*openaddressing_map)[id]->valid()) 
			return (*openaddressing_map)[id];
		remove_with_notification((*openaddressing_map)[id]->key, id);
		return nullptr;
	}

	size_t remove(const translated_key &key)
	{
		return inner_remove(key);
	}

	size_t remove(const unsigned int translated)
	{
		return inner_remove(translated);
	}

	~translation_unit()
	{
		for(auto &v: outerkey_translator)
		{
			delete v.second;
			v.second = nullptr;
		}
		delete openaddressing_map;
	}
private:

	size_t inner_remove(const translated_key &key)
	{
		auto element = outerkey_translator.find(key);
		if (element == outerkey_translator.end()) return 0;
		(*openaddressing_map)[element->second->id] = nullptr;
		size_t freed_size = element->second->size();
		delete element->second;
		outerkey_translator.erase(element);
		return freed_size;
	}

	size_t inner_remove(const unsigned int translated)
	{
		auto element = (*openaddressing_map)[translated];
		if (element == nullptr) return 0;
		size_t freed_size = element->size();
		(*openaddressing_map)[translated] = nullptr;
		outerkey_translator.erase(element->key);
		delete element;
		return freed_size;
	}


	void remove_with_notification(const translated_key &key, const unsigned int id)
	{
		size_t freed_size = (*openaddressing_map)[id]->size();
		auto expiry = (*openaddressing_map)[id]->expiry_time;
		element_removed_callback(key, id, expiry, freed_size);
		inner_remove(key);
	}

	std::unordered_map<translated_key, retrieved_element*, hasher> outerkey_translator;
	std::array<retrieved_element *, max_number_of_elements> *openaddressing_map = nullptr;
	element_removed_callback_t element_removed_callback;
};


#endif //DOORMAT_INTEGER_TRANSLATOR_H

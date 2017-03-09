#ifndef DOORMAT_TTL_ORDERED_H
#define DOORMAT_TTL_ORDERED_H

#include <chrono>
#include <iomanip>
#include <functional>
#include <cassert>

template<unsigned int max_elements_size = 1024*1024, unsigned int insord_threshold = 128>
class ttl_cleaner
{
public:
	using clock = std::chrono::system_clock;

private:
	size_t ordered_virtual_size = 0;
	size_t ordered_size = 0;
	size_t heap_size = 0;
	using remove_callback_t = std::function<void(const unsigned int id)>;
	remove_callback_t remove_callback;
	struct element
	{
		element() = default;
		element(clock::time_point expiry_time, int id) : expiry_time{expiry_time}, id{id} {}
		clock::time_point expiry_time;
		int id = -1;
		bool deleted = true;

		bool operator< (const element &other)
		{
			if(other.deleted) return true; //un elemento deleted è sempre maggiore, perché lob vogliamo spingere alla fine della coda.
			if (expiry_time < other.expiry_time) return true;
			if (expiry_time > other.expiry_time) return false;
			return id < other.id;
		}

		bool operator>(const element &other)
		{
			if(deleted) return true;
			if (expiry_time > other.expiry_time) return true;
			if (expiry_time < other.expiry_time) return false;
			return id > other.id;
		}

	};


	void heap_insert(const unsigned int id, const clock::time_point expiry_time)
	{
		if(heap_size == 0)
		{
			heap_ptr = ordered_set_ptr+ordered_size;
		}
		heap(heap_size).expiry_time = expiry_time;
		heap(heap_size).id = id;
		heap(heap_size).deleted = false;
		sift_up(heap_size);
		++heap_size;
	}


	inline int get_parent(const unsigned int pos) { return (pos -1)/2;}
	inline int get_left_child(const unsigned int pos) { return 2*pos+1;}
	inline int get_right_child(const unsigned int pos) { return 2*pos+2; }

	void sift_up(const unsigned int pos)
	{
		if(pos == 0) return;
		unsigned int parent_pos = get_parent(pos);
		if(heap(parent_pos) > heap(pos))
		{
			std::swap(heap(parent_pos),heap(pos));
			sift_up(parent_pos);
		}
	}


	void sift_down(const unsigned int pos)
	{
		if(pos >= heap_size) return;
		unsigned int left = get_left_child(pos);
		unsigned int right = get_right_child(pos);
		if(left >= heap_size || right >= heap_size || (heap(pos) < heap(left) && heap(pos) < heap(right))) return;
		unsigned int min_pos = (heap(left) < heap(right)) ? left : right;
		std::swap(heap(pos), heap(min_pos));
		sift_down(min_pos);
	}

	void heap_remove(const unsigned int pos)
	{
		if(!heap_ptr || !heap_size) return;
		heap(pos).deleted = true;
		std::swap(heap(pos), heap(heap_size-1));
		sift_down(pos);
		--heap_size;
	}

	element & element_at(element *ptr, const unsigned int pos)
	{
		size_t ptr_offset = ptr - &(*keeper)[0];

		return (*keeper)[(ptr_offset + pos)  % max_elements_size];
	}

	element& ordered_set(const unsigned int pos) { return element_at(ordered_set_ptr,pos); }
	element& heap(const unsigned int pos) { return element_at(heap_ptr, pos); }

	element& get_min_heap()
	{
		return heap(0);
	}

	void remove_min_heap()
	{
		heap_remove(0);
	}

	std::array<element, max_elements_size> *keeper = nullptr;
	element* heap_ptr = nullptr;
	element* ordered_set_ptr = nullptr;

	void heap_remove(const unsigned int id, const clock::time_point expiry_time)
	{
		if(!heap_ptr || !heap_size) return;
		unsigned int i = 0;
		while((i < heap_size && heap(i).expiry_time != expiry_time) || heap(i).id != static_cast<int>(id)) ++i;
		if(i != heap_size) {
			remove_callback(heap(i).id);
			heap_remove(i);
		}
	}

	int binary_search(unsigned int id, clock::time_point expiry_time, const unsigned int beginning, const unsigned int end)
	{
		if(beginning - end == 0) return -1;
		if(beginning - end == 1) {
			return (ordered_set(beginning).expiry_time == expiry_time
					&& ordered_set(beginning).id == static_cast<int>(id)) ? beginning : -1;
		}
		auto middle = (beginning + end)/2;
		int middle_search_off = 0;
		/** Skip deleted stuff. */
		while(middle + middle_search_off < end && middle - middle_search_off >= beginning)
		{
			if(!ordered_set(middle_search_off + middle).deleted)
			{
				middle += middle_search_off; break;
			}
			if(!ordered_set(middle_search_off - middle).deleted)
			{
				middle -= middle_search_off; break;
			}
			++middle_search_off;
		}
		if(middle - middle_search_off < beginning) {
			middle += middle_search_off;
			while(middle < end && ordered_set(middle).deleted) ++middle;
		}
		if(middle + middle_search_off == end)
		{
			middle -= middle_search_off;
			while(middle >= beginning && ordered_set(middle).deleted)
			{
				++middle;
			}
		}

		if(ordered_set(middle).deleted) return -1;

		if(ordered_set(middle).expiry_time == expiry_time && ordered_set(middle).id == static_cast<int>(id)) return middle;

		if(ordered_set(middle).expiry_time >= expiry_time || ordered_set(middle).id >= static_cast<int>(id))
		{
			return binary_search(id, expiry_time, beginning, middle);
		}

		return binary_search(id, expiry_time, middle, beginning);
	}

public:
	ttl_cleaner()
	{
		keeper = new std::array<element, max_elements_size>();
		ordered_set_ptr = &(*keeper)[0]; //the ordered set starts in the beginning.
	}

	~ttl_cleaner()
	{
		delete keeper;
	}

	void set_remove_callback(remove_callback_t && rc)
	{
		remove_callback = std::move(rc);
	}
	void merge() //warning: not in place merge. optimize later.
	{
		element *swapping_start = heap_ptr + heap_size;
		auto cur_ordered_pos = 0U;
		auto cur_writing_pos = 0U;
		while(heap_size && cur_ordered_pos < ordered_size)
		{
			element &min_ordered = ordered_set(cur_ordered_pos);
			element &min_heap = get_min_heap();
			if(min_ordered.deleted)
			{
				++cur_ordered_pos;
				continue;
			}
			if(min_ordered < min_heap)
			{
				element_at(swapping_start, cur_writing_pos++) = min_ordered;
				++cur_ordered_pos;
			} else
			{
				element_at(swapping_start, cur_writing_pos++) = min_heap;
				remove_min_heap();
			}
		}

		//one or the other of the two following while loops gets executed
		while(heap_size)
		{
			element &min_heap = get_min_heap();
			element_at(swapping_start, cur_writing_pos++) = min_heap;
			remove_min_heap();
		}
		while(cur_ordered_pos < ordered_size)
		{
			element_at(swapping_start, cur_writing_pos++) = ordered_set(cur_ordered_pos++);
		}

		ordered_set_ptr = swapping_start;
		heap_size= 0;
		heap_ptr = nullptr;
		ordered_size = cur_writing_pos;
		ordered_virtual_size = ordered_size;
	}

	void insert(unsigned int id, clock::time_point expiry_time)
	{
		assert(ordered_size + heap_size < max_elements_size);
		//we've not started an heap, and we can insert in order: just do it.
		if(!heap_size)
		{
			ordered_set(ordered_size).expiry_time = expiry_time;
			ordered_set(ordered_size).id = id;
			ordered_set(ordered_size).deleted = false;

			if(ordered_size && expiry_time >= ordered_set(ordered_size-1).expiry_time
					&& static_cast<int>(id) >= ordered_set(ordered_size-1).id)
			{
				++ordered_size;
				++ordered_virtual_size;
				return;
			}
			if(ordered_size < insord_threshold)
			{
				//then the rest is ordered; hence we just find the position and go on swapping
				int j = ordered_size;
				while(j > 0 && ordered_set(j-1) > ordered_set(j))
				{
					std::swap(ordered_set(j-1), ordered_set(j));
					--j;
				}
				while(ordered_size >= 0 && ordered_set(ordered_size).deleted) { ordered_size--;}
				++ordered_size;
				++ordered_virtual_size;
				return;
			}
		}
		heap_insert(id, expiry_time);
		size_t free_space = max_elements_size - (heap_size + ordered_size);
		if(free_space == ordered_size + heap_size || heap_size >= ordered_virtual_size) merge();
	}

	bool lazy_remove_element(unsigned int id, clock::time_point expiry_time)
	{
		int element_pos = binary_search(id, expiry_time, 0, ordered_size);
		if(element_pos >= 0) {
			remove_callback(ordered_set(element_pos).id);
			ordered_set(element_pos).deleted = true;
			ordered_virtual_size--;
		}
		return element_pos >= 0;
	}


	void thorough_remove_element(unsigned int id, clock::time_point expiry_time)
	{
		if(!lazy_remove_element(id, expiry_time)) {
			heap_remove(id, expiry_time);
		}
	}

	void thorough_remove_element(unsigned int id)
	{
		for(size_t  i = 0; i < ordered_size; ++i)
		{
			if(ordered_set(i).id == static_cast<int>(id))
			{
				ordered_set(i).deleted = true;
				ordered_virtual_size--;
				return;
			}
		}

		for(size_t i = 0; i < heap_size; ++i)
		{
			if(heap(i).id == static_cast<int>(id))
			{
				heap_remove(i);
				return;
			}
		}
	}

	void print(clock::time_point reference_time)
	{
		for(size_t i = 0; i < ordered_size; ++i)
		{
			auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(ordered_set(i).expiry_time - reference_time);
			std::cout << diff.count() << "," << ordered_set(i).id << "," <<  ((ordered_set(i).deleted) ? 'D' : 'A') << "\t";
		}
		std::cout << std::endl;
	}

	void print_heap(clock::time_point reference_time)
	{
		for(size_t i = 0; i < heap_size; ++i)
		{
			auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(heap(i).expiry_time - reference_time);
			std::cout << diff.count() << "," << heap(i).id << "," <<  ((heap(i).deleted) ? 'D' : 'A') << "\t";
		}
		std::cout << std::endl;
	}

	void remove_expired(clock::time_point tp = {})
	{
		auto current_time = tp == clock::time_point{} ? clock::now() : tp;
		size_t i = 0;
		while(i < ordered_size && ordered_set(i).expiry_time <= current_time)
		{
			ordered_set(i).deleted = true;
			remove_callback(ordered_set(i).id);
			++i;
		}
		ordered_size-=i;
		ordered_virtual_size-=i;
		//distanza dalla base 			+ elementi cancellati = nuovo punto di inizio.
		ordered_set_ptr = &element_at(ordered_set_ptr, i);
	}
};


#endif //DOORMAT_TTL_ORDERED_H

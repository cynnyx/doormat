#ifndef DOORMAT_MEMORY_SUPPLIER_H_
#define DOORMAT_MEMORY_SUPPLIER_H_

#include <list>
#include <cstdint>
#include <type_traits>

#include "../service_locator/service_locator.h"
#include "../memory/buffer.h"

namespace utils
{

/**
 * @brief The memory_supplier provides chunks on request
 * and holds the ownership of the underlying memory.
 */
template<typename Unit = const uint8_t>
class memory_supplier
{
public:
	/**
	 * @brief get_chunk return a new chunk of writable memory of the
	 * requested size.
	 *
	 * The chunk is requested to an underlying buffer; other buffers
	 * are allocated as needed.
	 *
	 * @param size the size of the requested chunk
	 * @return a chunk of the requested size, if it was
	 * possible to fulfil the request; a null chunk otherwise.
	 */
	memory::chunk<Unit> get_chunk(size_t size)
	{
		if(buffers_.empty()||buffers_.back().free_size() < size)
			buffers_.emplace_back(service::locator::allocator().get_buffer<typename decltype(buffers_)::value_type>());
		return buffers_.back().mem_to_write(size);
	}

	memory::buffer<Unit>& buffer()
	{
		if(buffers_.empty() || buffers_.back().free_size() == 0)
			buffers_.emplace_back(service::locator::allocator().get_buffer<typename decltype(buffers_)::value_type>());
		return buffers_.back();
	}

private:
	std::list<memory::buffer<Unit>> buffers_;
};


} // namespace utils



#endif // DOORMAT_MEMORY_SUPPLIER_H_

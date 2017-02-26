#include "http2alloc.h"

#include <cstdlib>

// #include "../utils/logWrapper.h"

namespace http2
{

void* malloc_cb(size_t size, void *mem_user_data) 
{
	http2_allocator<std::uint8_t>* ha = static_cast<http2_allocator<std::uint8_t>*>( mem_user_data );
	void* r = ha->allocate( size );
// 	logTrace ( "Malloc ", r, " align ", ((long) r)%8 );
	return r;
}

void free_cb(void *ptr, void *mem_user_data) 
{
// 	logTrace ( "Free ", ptr, " align ", ((long) ptr)%8  );
	http2_allocator<std::uint8_t>* ha = static_cast<http2_allocator<std::uint8_t>*>( mem_user_data );
	ha->deallocate( static_cast<http2_allocator<std::uint8_t>::pointer>( ptr ), 0 );
}

void* calloc_cb(size_t nmemb, size_t size, void *mem_user_data) 
{
// 	logTrace("Calloc: ", nmemb, " ", size );
	size_t real_size = size * nmemb;
	void* allocated = malloc_cb( real_size, mem_user_data );
	std::memset( allocated, 0, real_size );
// 	logTrace("Calloc: ", allocated );
	return allocated;
}

void* realloc_cb(void *ptr, size_t size, void *mem_user_data) 
{
// 	logTrace("Realloc ", ptr, " size ", size );
	http2_allocator<std::uint8_t>* ha = static_cast<http2_allocator<std::uint8_t>*>( mem_user_data );
	void* r = ha->realloc( static_cast<http2_allocator<std::uint8_t>::pointer>( ptr ), size );
// 	logTrace( "Realloc ", r );
	return r;
}

static http2_allocator<std::uint8_t> global_allocator;

nghttp2_mem create_allocator()
{
	return nghttp2_mem{ &global_allocator, malloc_cb, free_cb, calloc_cb, realloc_cb };
}

}

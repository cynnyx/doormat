#pragma once

#include <cstdint>
#include <new>
#include <cstring>
#include <cstddef>

#include <nghttp2/nghttp2.h>
namespace http2
{

nghttp2_mem create_allocator();

// This is a skeleton of allocator to be used with nghttp2_mem
// For the future: constructor copy *must* not throw
template<class T>
class http2_allocator
{
public:
	using size_type = std::size_t;
	using difference_type = std::ptrdiff_t;
	using pointer = T*;
	using const_pointer = const T*;
	using reference = T&;
	using const_reference = const T&;
	using value_type = T;
	
	template<typename U>
	struct rebind { using other = http2_allocator<U>; };

	pointer allocate( std::size_t n, const void * hint = nullptr )
	{
		//return static_cast<T*>(::operator new(n * sizeof(T), std::nothrow ));
		return static_cast<pointer>( ::malloc( n ) );
	}
	
	void deallocate(pointer p, size_type n)
	{
		//::operator delete(p);
		::free( p );
	}
	
	pointer realloc( pointer p, size_type size )
	{
		return static_cast<pointer>( ::realloc( p, size ) );
	}
	
// 	template<typename U, typename ... Args>
// 	void construct(U* p, Args&&... args)
// 	{
// 		new (p) U ( std::forward<Args>( args)... );
// 	};
// 
// 	template<typename U>
// 	void destroy(U* p)
// 	{
// 		p->~U();
// 	}

	constexpr bool operator==( const http2_allocator& ) const noexcept { return true; }
	constexpr bool operator!=( const http2_allocator& o ) const noexcept { return ! ( o == *this ); }
};
}

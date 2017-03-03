#pragma once

#include "log_wrapper.h"

#include <cassert>
#include <boost/asio/buffer.hpp>

template <int N>
class reusable_buffer
{
	bool bound{false};
	char mem[N];
	char* wr;
	const char* rd;

	size_t avail_mem()
	{
		if( wr < rd )
			return rd - wr;

		if( wr > rd )
			return N + mem - wr;

		if( bound )
			return 0;

		//Re-align pointers
		rd = wr = mem;
		return N;
	}

public:
	reusable_buffer()
		: mem{}
		, wr{mem}
		, rd{mem}
	{
		//LOGTRACE(this, "constructor");
	}

	~reusable_buffer()
	{
		//LOGTRACE(this, "destructor");
	}

	boost::asio::mutable_buffer reserve()
	{
		return boost::asio::buffer(wr, avail_mem());
	}

	char* produce(const size_t len)
	{
		char* ptr{nullptr};
		const size_t& max = avail_mem();
		if( len <= max )
		{
			ptr = wr;
			wr += len;
			if( wr == mem + N )
			{
				bound = true;
				wr = mem;
			}
		}
		return ptr;
	}

	size_t consume(const char* ptr)
	{
		size_t bytes{0};
		const char* upper_bound = bound?mem + N: wr;
		if(ptr > mem && ptr <= upper_bound)
		{
			bytes = ptr - rd;
			rd = ptr;

			if(rd == mem + N)
			{
				bound = false;
				rd = mem;
			}
		}
		return bytes;
	}
};

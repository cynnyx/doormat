#include "dstring_factory.h"

#include <cassert>

dstring_factory::dstring_factory(const size_t len, const bool caseins)
	: _size{len}
	, _caseins{caseins}
{}

dstring_factory::~dstring_factory()
{
	if(_data)
		delete _data;
}

char* dstring_factory::data() noexcept
{
	if(!_data)
		_data = new char[_size];
	return _data;
}

size_t dstring_factory::max_size() const noexcept
{
	return _size;
}

dstring dstring_factory::create_dstring(const size_t len) noexcept
{
	assert(len <= _size);
	dstring d;
	if(_data)
	{
		if(len)
			d.set_valid();
		if(_caseins)
			d.set_ci();

		d._size = len;
		d._capacity = _size;
		d._data = _data;
		_data = nullptr;
	}
	return d;
}

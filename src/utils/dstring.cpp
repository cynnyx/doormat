#include "dstring.h"
#include "log_wrapper.h"

#include <assert.h>
#include <cstring>
#include <iostream>

constexpr size_t dstring::npos;

const dstring dstring::make_immutable(const char* c, const bool caseins) noexcept
{
	return dstring{c, strlen(c), caseins, true};
}

dstring::dstring(const bool caseins) noexcept
	: _flags{0}
	, _size{0}
	, _capacity{0}
	, _refcount{new uint(0)}
	, _data{nullptr}
{
	set_ci(caseins);
}

dstring::dstring(size_t t, const bool caseins) noexcept
	: _flags{0}
	, _size{t}
	, _capacity{t}
	, _refcount{new uint(0)}
	, _data{new char[t]}
{
	set_ci(caseins);
}

dstring::dstring(const char* c , size_t len, const bool caseins, const bool) noexcept
	: _size{len}
	, _capacity{len}
	, _data{new char[len]}
{
	set_immutable(true);
	set_valid(len);

	if(caseins)
	{
		set_ci();
		std::transform(c, c+len, _data, ::tolower);
	}
	else
		std::copy_n(c, len, _data);
}

dstring::dstring(const dstring& other) noexcept
	: _flags{other._flags}
	, _size{other._size}
	, _capacity{other._capacity}
	, _refcount{other._refcount}
	, _data{other._data}
{
	set_immutable(false);
	if( _refcount )
		*_refcount +=1;
}

dstring::dstring(dstring&& other) noexcept
{
	std::swap(_flags, other._flags);
	std::swap(_size, other._size);
	std::swap(_capacity, other._capacity);
	std::swap(_refcount, other._refcount);
	std::swap(_data, other._data);
}

dstring::dstring(const char* c , size_t len, const bool caseins) noexcept
	: dstring{len, caseins}
{
	set_valid(len);

	if(caseins)
	{
		set_ci();
		std::transform(c, c+len, _data, ::tolower);
	}
	else
		std::copy_n(c, len, _data);
}

//Ctors below map on one of the above
dstring::dstring(const char* c, const bool caseins) noexcept
	: dstring{c, strlen(c), caseins}
{}

dstring::dstring(const unsigned char* c, const bool caseins) noexcept
	: dstring{reinterpret_cast<const char*>(c), caseins}
{}

dstring::dstring(const unsigned char* c , size_t len, const bool caseins) noexcept
	: dstring{reinterpret_cast<const char*>(c), len, caseins}
{}

dstring::~dstring() noexcept
{
	reset();
}

void dstring::reset()
{
	if(!_refcount && is_immutable())
	{
		delete[] _data;
		return;
	}

	if( _refcount )
	{
		if( *_refcount > 0)
		{
			*_refcount -= 1;
		}
		else
		{
			delete _refcount;
			delete[] _data;
		}
	}
}

dstring::operator std::string() const noexcept
{
	return is_valid() ? std::string{_data, _size} : "";
}

dstring& dstring::operator=(const dstring& other) noexcept
{
	reset();
	_flags = other._flags;
	set_immutable(false);

	_size = other._size;
	_capacity = other._capacity;
	_data = other._data;
	_refcount = other._refcount;

	if(_refcount)
		*_refcount +=1;

	return *this;
}

dstring& dstring::operator=(dstring&& other) noexcept
{
	std::swap(_flags, other._flags);
	std::swap(_size, other._size);
	std::swap(_capacity, other._capacity);
	std::swap(_data, other._data);
	std::swap(_refcount, other._refcount);
	return *this;
}

dstring dstring::clone(std::function<char(char)> fn) const noexcept
{
	if(fn)
	{
		dstring d{_size};
		std::transform(cbegin(), cend(), d._data, fn);
		d._flags = _flags;
		d.set_immutable(false);
		return d;
	}

	return dstring{_data, _size};
}

dstring dstring::substr(const_iterator b , const_iterator e, std::function<char(char)> fn) const noexcept
{
	size_t s = std::distance(b,e);
	dstring d{s};

	if(fn)
		std::transform(b,e, d._data, fn);
	else
		std::copy(b,e,d._data);

	//FIXME:should case insensitivity be propagated?
	if(s)
		d.set_valid();
	return d;
}

dstring dstring::substr(size_t pos, size_t count, std::function<char(char)> fn) const noexcept
{
	if(count < size())
		return substr(cbegin()+pos, cbegin()+pos+count, fn);
	else
		return substr(cbegin()+pos, cend(), fn);
}

bool dstring::operator==( const dstring& other) const  noexcept
{
	if(_size != other._size)
		return false;

	if( is_valid() != other.is_valid() )
		return false;

	//Two empty strings should compare equal
	if( _data != other._data && ( !_data || !other._data ))
		return false;

	return _data == other._data || std::equal(cbegin(), cend(), other.cbegin());
}

bool dstring::operator==(const std::string& other) const noexcept
{
	if(_size != other.size())
		return false;
	return _data == other.data() || std::equal(cbegin(), cend(), other.data());
}

bool dstring::operator==(const char* other) const noexcept
{
	if(_size != strlen(other))
		return false;
	return _data == other || std::equal(cbegin(), cend(), other);
}

bool dstring::operator!=( const dstring& other) const noexcept
{
	return !operator==(other);
}

bool dstring::operator<( const dstring& other) const noexcept
{
	return std::lexicographical_compare(cbegin(), cend(), other.cbegin(), other.cend());
}

const char& dstring::front() const noexcept
{
	return _data[0];
}

const char& dstring::back() const noexcept
{
	return _data[_size-1];
}

dstring::const_iterator dstring::cbegin() const noexcept
{
	return _data;
}

dstring::const_iterator dstring::cend() const noexcept
{
	return _data + _size;
}

bool dstring::empty() const noexcept
{
	return _size == 0;
}

size_t dstring::size() const noexcept
{
	return _size;
}

size_t dstring::find(char c, size_t pos) const noexcept
{
	size_t rv{npos};
	auto hit = std::find(_data + pos, _data + _size, c);
	if( hit != cend())
		rv = hit - _data;
	return rv;
}

size_t dstring::find(const char* data, size_t len, size_t pos) const noexcept
{
	const char* it = data;
	const char* end = data + len;

	for(size_t i = pos; i < _size; ++i)
	{
		size_t innerscanner = i;
		while(_data[innerscanner] == *it && innerscanner < _size && it != end)
		{
			++innerscanner;
			++it;
		}

		//Hit
		if(it == end)
			return i;

		//Miss + Nothing left
		if(innerscanner == _size)
			break;
	}
	return npos;
}

dstring& dstring::append(const char* ndata, size_t len) noexcept
{
	if(len != 0 && is_immutable() != true)
	{
		assert(_size <= _capacity);

		size_t rsize {_size + len};
		size_t ncapacity = _capacity > 0? _capacity: rsize;

		while(ncapacity < rsize)
			ncapacity *=2;

		if(ncapacity > _capacity)
		{
			char *tmp = new char[ncapacity];
			_capacity = ncapacity;

			if( is_valid() )
			{
				if( is_ci() )
					std::transform(_data, _data + _size, tmp, ::tolower );
				else
					std::copy_n(_data, _size, tmp);
			}
			else
			{
				assert(_data == nullptr);
				assert(_size == 0);
			}

			if(!_refcount || *_refcount)
				_refcount = new uint(0);
			else
				delete[] _data;

			_data = tmp;
		}

		set_valid();
		set_immutable(false);

		if( is_ci() )
			std::transform(ndata, ndata + len, _data + _size, ::tolower );
		else
			std::copy_n(ndata, len, _data + _size);
		_size = rsize;
	}
	return *this;
}

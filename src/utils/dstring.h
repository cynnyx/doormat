#pragma once

#include <string>
#include <functional>
#include <algorithm>
#include <type_traits>
#include <cctype>
#include <cstring>

class dstring
{
	friend class dstring_factory;

	enum flags : uint8_t
	{
		valid = 1,
		cinsensitive = 1<<1,
		immutable = 1<<2
	};

	uint8_t _flags{0};
	size_t _size{0};
	size_t _capacity{0};
	uint* _refcount{nullptr};
	char* _data{nullptr};

	void reset();
	explicit dstring(size_t t, const bool caseins = false) noexcept;
	explicit dstring(const char* c , size_t, const bool, const bool) noexcept;

	void set_valid(bool val = true) noexcept
	{
		if(val)
			_flags |= flags::valid;
		else
			_flags &= ~flags::valid;
	}

	void set_ci(bool val = true) noexcept
	{
		if(val)
			_flags |= flags::cinsensitive;
		else
			_flags &= ~flags::cinsensitive;
	}

	void set_immutable(bool val = true) noexcept
	{
		if(val)
			_flags |= flags::immutable;
		else
			_flags &= ~flags::immutable;
	}

	bool is_valid() const noexcept { return _flags&flags::valid; }
	bool is_ci() const noexcept { return _flags&flags::cinsensitive; }
	bool is_immutable() const noexcept { return _flags&flags::immutable; }
public:
	static constexpr size_t npos = static_cast<size_t>(-1);

	using iterator = char *;
	using const_iterator = const char *;

	template<typename T>
	static dstring to_string(const T& t)
	{
		auto tmp = std::to_string(t);
		return dstring{tmp.data(), tmp.size()};
	}

	static const dstring make_immutable(const char*, const bool caseins = false) noexcept;

	dstring(const bool caseins = false) noexcept;
	dstring(const char*, size_t, const bool caseins = false) noexcept;
	dstring(const dstring&) noexcept;
	dstring(dstring&&) noexcept;

	dstring(const char*, const bool caseins = false) noexcept;
	dstring(const unsigned char*, const bool caseins = false) noexcept;
	dstring(const unsigned char*, size_t, const bool caseins = false) noexcept;
	~dstring();

	dstring& operator=( const dstring& ) noexcept;
	dstring& operator=( dstring&& ) noexcept;

	bool operator!=( const dstring& ) const noexcept;
	bool operator<( const dstring& d) const noexcept;

	bool operator==(const dstring&) const noexcept;
	bool operator==(const std::string&) const noexcept;
	bool operator==(const char*) const noexcept;

	/**
	 * @warning What operator will be called?
	 * If you write std::string{ ... } a bool, so use ( ) or static_cast
	 */
	operator bool() const noexcept { return is_valid(); }
	operator std::string() const noexcept;

	const char* cdata() const noexcept { return _data; }

	const char& front() const noexcept;
	const char& back() const noexcept;

	const_iterator cbegin() const noexcept;
	const_iterator cend() const noexcept;

	bool empty() const noexcept;
	size_t size() const noexcept;

	size_t find(char c, size_t pos = 0) const noexcept;
	size_t find(const char* data, size_t len, size_t pos = 0) const noexcept;
	size_t find(const char *cstr, size_t pos = 0) const noexcept{ return find(cstr, strlen(cstr), pos); }
	size_t find(const dstring &ds, size_t pos = 0) const noexcept { return find(ds._data, ds._size, pos); }
	size_t find(const std::string &str, size_t pos = 0) const noexcept { return find(str.data(), str.size(), pos); }

	dstring& append(const char* data, size_t len) noexcept;
	dstring& append(const char* data) noexcept { return append(data, strlen(data)); }
	dstring& append(const dstring& ds) noexcept { return append(ds._data, ds._size); }
	dstring& append(const std::string str) noexcept { return append(str.data(), str.size()); }

	template<typename T>
	bool to_integer(T& t) const noexcept
	{
		bool rv{false};
		t = 0;

		//Trim leading whitespaces
		auto b = std::find_if_not(cbegin(),cend(), ::isspace);

		if( b != cend() )
		{
			int8_t sign{1};
			if( *b == '-' )
			{
				sign = -1;
				b++;
			}

			auto toi = [&t](const char& c)
			{
				if(std::isdigit(c) && t >= 0)
				{
					t *= 10;
					t += c - '0';
					return true;
				}
				return false;
			};

			if( std::all_of(b, cend(), toi) )
			{
				rv = true;
				t *=sign;
			}
		}
		return rv;
	}

	dstring clone(std::function<char(char)> fn = nullptr) const noexcept;
	dstring substr(const_iterator b , const_iterator e, std::function<char(char)> fn = nullptr) const noexcept;
	dstring substr(size_t pos, size_t count = npos, std::function<char(char)> fn = nullptr) const noexcept;
};

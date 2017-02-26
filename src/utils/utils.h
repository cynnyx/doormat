#ifndef UTILS__H
#define UTILS__H

#include <memory>
#include <string>
#include <string.h>
#include <sstream>
#include <algorithm>
#include <iterator>
#include <functional>

namespace utils
{

template<typename... Args>
std::string stringify(Args&&... args)
{
	std::stringstream ss;
	int a[] = { (ss << args, 0)... };
	(void)a;

	return ss.str();
}

template<typename T1, typename T2>
struct equal_to
{
	bool operator()(const T1& t1, const T2& t2)
	{
		return t1 == t2;
	}
};

std::string get_first_part( const std::string& s, char delimiter );

template<class T>
T to( const std::string& s )
{
	static_assert( std::is_integral<T>::value == true || ! std::is_same<T, bool>::value == false, "Use it only with real integers!" );

	std::stringstream ss{ s };
	T rval;
	ss >> rval;
	return rval;
}

template<class T>
std::string from( T integer)
{
	static_assert( std::is_integral<T>::value == true || std::is_same<T, bool>::value == false, "Use it only with real integers!" );

	std::stringstream ss;
	ss << std::hex << integer;
	return ss.str();
}

template<class T>
std::string from_dec( T integer)
{
	static_assert( std::is_integral<T>::value == true || std::is_same<T, bool>::value == false, "Use it only with real integers!" );

	std::stringstream ss;
	ss << std::dec << integer;
	return ss.str();
}

template<typename T, typename... Args>
std::unique_ptr<T> make_unique(Args&&... args)
{
	return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

bool icompare( const std::string& a, const std::string& b );

template <class T>
struct iless
{
	bool operator() (const T& x, const T& y) const
	{
		bool rv{false};

		auto iless_comparator = []( const char& a, const char& b )
			{return std::tolower(a) <= std::tolower(b);};

		if( x.size() <= y.size() )
			rv = std::equal(x.cbegin(), x.cend(), y.cbegin(), iless_comparator);
		else
			rv = !std::equal(y.cbegin(), y.cend(), x.cbegin(), iless_comparator);
		return rv;
	}
};

template <typename T1, typename T2>
bool i_starts_with(const T1& a, const T2& b)
{
	if( b.size() <= a.size() )
	{
		return std::equal(b.cbegin(), b.cend(), a.cbegin(),
			[](const char& ca, const char& cb){ return ::tolower(ca) == ::tolower(cb); });
	}
	return false;
}

// TODO: various starts_with / ends_with implementation should clearly state how
// they behave with empty strings / containers

template <typename InputIterator1,
		  typename InputIterator2,
		  typename BinaryPredicate = equal_to<decltype(*std::declval<InputIterator1>()),decltype(*std::declval<InputIterator2>())>>
bool starts_with(InputIterator1 first1, InputIterator1 last1,
				 InputIterator2 first2, InputIterator2 last2,
				 BinaryPredicate p = {})
{
	if (last1 - first1 < last2 - first2)
	{
		return false;
	}

	return std::equal(first2, last2, first1, p);
}

template <typename S,
		  typename T,
		  typename BinaryPredicate = equal_to<decltype(*std::begin(std::declval<S>())),decltype(*std::begin(std::declval<T>()))>>
bool starts_with(const S &a,const T &b, BinaryPredicate p = {})
{
	return starts_with(a.begin(), a.end(), b.begin(), b.end(), p);
}


/**
 * @brief starts_with checks whether a string starts with a certain _substring_.
 * @param str the string to be checked
 * @param substr the constant char array to check str against
 * @return false if str is shorter than substr or if their overlapping differs,
 * true otherwise
 */
template<size_t N, typename BinaryPredicate = std::equal_to<char>>
bool starts_with(const std::string& str, const char (& substr)[N], size_t str_offset = 0, BinaryPredicate p = {}) noexcept
{
	static_assert(N > 0, "you have not passed a valid string");
	if(N-1 > str.size() - str_offset)
		return false;

	return std::equal(std::begin(substr), std::end(substr) - 1, str.begin() + str_offset, p);
}

/**
 * @brief starts_with checks whether a string starts with a certain _substring_.
 * @param str the string to be checked
 * @param substr the string to check str against
 * @return false if str is shorter than substr or if their overlapping differs,
 * true otherwise
 */
template<typename BinaryPredicate = std::equal_to<char>>
bool starts_with(const std::string& str, const std::string& substr, BinaryPredicate p = {}) noexcept
{
	if(substr.size() > str.size())
		return false;

	return std::equal(substr.begin(), substr.end(), str.begin());
}


/**
 * @brief ends_with checks whether a string ends with a certain _substring_.
 * @param str the string to be checked
 * @param substr the constant char array to check str against
 * @return false if str is shorter than substr or if their overlapping differs,
 * true otherwise
 */
template<size_t N, typename BinaryPredicate = std::equal_to<char>>
bool ends_with(const std::string& str, const char (& substr)[N], BinaryPredicate p = {}) noexcept
{
	static_assert(N > 0, "you have not passed a valid string");
	if(N-1 > str.size())
		return false;

	auto substr_rbeg = std::reverse_iterator<decltype(std::begin(substr))>(std::end(substr) - 1);
	auto substr_rend = std::reverse_iterator<decltype(std::begin(substr))>(std::begin(substr));
	return std::equal(substr_rbeg, substr_rend, str.rbegin(), p);
}

/**
 * @brief ends_with checks whether a string ends with a certain _substring_.
 * @param str the string to be checked
 * @param substr the string to check str against
 * @return false if str is shorter than substr or if their overlapping differs,
 * true otherwise
 */
template<typename BinaryPredicate = std::equal_to<char>>
bool ends_with(const std::string& str, const std::string& substr, BinaryPredicate p = {}) noexcept
{
	if(substr.size() > str.size())
		return false;

	return std::equal(substr.rbegin(), substr.rend(), str.rbegin(), p);
}

template<typename container>
std::string to_lower_hex(const container& in)
{
	static const char* digits = "0123456789abcdef";
	static const uint8_t mask{0x0f};

	std::string out(in.size()<<1,0);
	size_t idx{0};
	for(const uint8_t& c : in)
	{
		out[idx++] = digits[(c>>4)&mask];
		out[idx++] = digits[c&mask];
	}
	return out;
}

std::string hostname_from_url( const std::string& uri );

}//utils
#endif

#include <gtest/gtest.h>

#include "../src/configuration/configuration_wrapper.h"
#include "../src/configuration/configuration_parser.h"

#include <array>

#include "boost/asio.hpp"


namespace
{

class my_ipv4
{
	std::array<uint8_t,4> bytes;
public:
	my_ipv4(uint8_t a = 0, uint8_t b = 0, uint8_t c = 0, uint8_t d = 0)
		: bytes{a, b, c, d}
	{}

	my_ipv4(const my_ipv4& i)
	{
		std::copy(i.bytes.begin(), i.bytes.end(), bytes.begin());
	}

	my_ipv4(uint32_t i)
	{
		bytes[3] = i & 0xff;
		bytes[2] = i >> 8 & 0xff;
		bytes[1] = i >> 16 & 0xff;
		bytes[0] = i >> 24 & 0xff;
	}

	my_ipv4& operator++()
	{
		if(++bytes[3] == 0)
			if(++bytes[2] == 0)
				if(++bytes[1] == 0)
					++++bytes[0];
		return *this;
	}

	my_ipv4 operator++(int)
	{
		auto ret = *this;
		++*this;
		return ret;
	}

	operator std::string() const
	{
		return std::to_string(bytes[0]).append(".")
				.append(std::to_string(bytes[1])).append(".")
				.append(std::to_string(bytes[2])).append(".")
				.append(std::to_string(bytes[3]));
	}
};

std::ostream& operator<<(std::ostream& os, const my_ipv4& ip) { return os << std::string(ip); }

}


/*
 * We intend to test the
 */
TEST(privileged_ipv4_matcher, usage)
{
	configuration_parser parser{"./etc/doormat/doormat.ipv4_matcher_test.config"};
	auto cf = parser.parse();
	auto &cfg = *cf;

	const auto n = 10; // number of x.0.0.0/yy in the ip_addresses file
	const auto max_iter = 1U << (n + 2); // number of addresses to check for every iteration (much more than _bound_ value)
	for(uint32_t i = 0; i < n; ++i)
	{
		const uint32_t bound = 1U << i; // the number of addresses that should return TRUE
		my_ipv4 ip(i << 24); // inizialize the ip with the pattern i.0.0.0
		for(uint32_t j = 0; j < bound; ++j)
			EXPECT_TRUE(cfg.privileged_ipv4_matcher(ip++)) << "i == " << i << ", j == " << j << "; ip == " << ip;
		for(uint32_t j = bound; j < max_iter; ++j)
			EXPECT_FALSE(cfg.privileged_ipv4_matcher(ip++)) << "i == " << i << ", j == " << j << "; ip == " << ip;
	}
}

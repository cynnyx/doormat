#include <gtest/gtest.h>

#include <vector>

#include "../src/utils/sni_solver.h"

using namespace ssl_utils;

static const std::vector<std::vector<std::string>> certificates =
{
	{
		"etc/doormat/certificates/npn/newcert.pem",
		"etc/doormat/certificates/npn/newkey.pem",
		"etc/doormat/certificates/npn/keypass"
	},
	{
		"etc/doormat/certificates/npn1/newcert.pem",
		"etc/doormat/certificates/npn1/newkey.pem",
		"etc/doormat/certificates/npn1/keypass"
	}
};

TEST(sni_solver, valid)
{
	sni_solver tested;
	
	for ( auto&& cert : certificates )
	{
		tested.add_certificate( cert[0], cert[1], cert[2] );
	}
	ASSERT_TRUE(tested.load_certificates());
	ASSERT_EQ((unsigned int) std::distance(tested.begin(), tested.end()), certificates.size());
}

TEST(sni_solver, invalid)
{
	sni_solver tested;
	
	tested.add_certificate("pi","vu","elle");

	ASSERT_FALSE(tested.load_certificates());
	ASSERT_EQ( 0U, std::distance(tested.begin(), tested.end()));
}



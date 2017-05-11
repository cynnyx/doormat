#include <gtest/gtest.h>

#include <vector>

#include "../src/utils/sni_solver.h"

using namespace ssl_utils;

static const std::vector<std::vector<std::string>> certificates =
{
	{
		"etc/doormat/certificates/npn/newcert.pem",
		"etc/doormat/certificates/npn/newkey.pem",
		"pippo123"
	},
	{
		"etc/doormat/certificates/npn1/newcert.pem",
		"etc/doormat/certificates/npn1/newkey.pem",
		"pippo123"
	}
};

TEST(sni_solver, valid)
{
	sni_solver tested;
	
	for ( auto&& cert : certificates )
	{
		bool ok = tested.load_certificate( cert[0], cert[1], cert[2] );
		ASSERT_TRUE( ok );
	}
	ASSERT_EQ( tested.size(), certificates.size() );
}

TEST(sni_solver, invalid)
{
	sni_solver tested;
	
	bool ok = tested.load_certificate("pi","vu","elle");
	ASSERT_FALSE(ok);
	ASSERT_EQ( 0U, tested.size() );
}



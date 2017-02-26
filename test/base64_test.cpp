#include <gtest/gtest.h>

#include "../src/utils/base64.h"

static std::string base64_s = "V29vbywgYSBoYXBweSBzdHJpbmc=";
static const unsigned char* base_string = reinterpret_cast<const unsigned char*> ( "Wooo, a happy string" );

TEST(base64, encoding)
{
	dstring r = utils::base64_encode( base_string, strlen( reinterpret_cast<const char*>(base_string) ) );
	std::string cr{ r.cdata(), r.size() };
	ASSERT_EQ( cr, base64_s );
}

TEST(base64, decoding)
{
	dstring r = utils::base64_decode( base64_s );
	std::string cr{ r.cdata(), r.size() };
	ASSERT_EQ( cr, reinterpret_cast<const char*> ( base_string ) );
}

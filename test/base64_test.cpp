#include <gtest/gtest.h>

#include "../src/utils/base64.h"

static std::string base64_s = "V29vbywgYSBoYXBweSBzdHJpbmc=";
static const unsigned char* base_string = reinterpret_cast<const unsigned char*> ( "Wooo, a happy string" );

TEST(base64, encoding)
{
	auto r = utils::base64_encode( base_string, strlen( reinterpret_cast<const char*>(base_string) ) );
	ASSERT_EQ( r, base64_s );
}

TEST(base64, decoding)
{
	auto r = utils::base64_decode( base64_s );
	ASSERT_EQ( r, reinterpret_cast<const char*> ( base_string ) );
}

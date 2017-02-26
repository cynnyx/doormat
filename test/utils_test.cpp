#include <gtest/gtest.h>

#include "../src/utils/utils.h"

TEST(Utils, StartsWithArrayOfChar)
{
	// TODO test against / with empty string
	std::string str("Test string");

	EXPECT_TRUE(utils::starts_with(str, "T"));
	EXPECT_TRUE(utils::starts_with(str, "Te"));
	EXPECT_TRUE(utils::starts_with(str, "Tes"));
	EXPECT_TRUE(utils::starts_with(str, "Test"));
	EXPECT_TRUE(utils::starts_with(str, "Test "));
	EXPECT_TRUE(utils::starts_with(str, "Test s"));
	EXPECT_TRUE(utils::starts_with(str, "Test st"));
	EXPECT_TRUE(utils::starts_with(str, "Test str"));
	EXPECT_TRUE(utils::starts_with(str, "Test stri"));
	EXPECT_TRUE(utils::starts_with(str, "Test strin"));
	EXPECT_TRUE(utils::starts_with(str, "Test string"));

	EXPECT_FALSE(utils::starts_with(str, " T"));
	EXPECT_FALSE(utils::starts_with(str, "te"));
	EXPECT_FALSE(utils::starts_with(str, "tes "));
	EXPECT_FALSE(utils::starts_with(str, "Test-"));
	EXPECT_FALSE(utils::starts_with(str, "Test\n"));
	EXPECT_FALSE(utils::starts_with(str, "test s"));
	EXPECT_FALSE(utils::starts_with(str, "Test St"));
	EXPECT_FALSE(utils::starts_with(str, "Test stt"));
	EXPECT_FALSE(utils::starts_with(str, "Test srti"));
	EXPECT_FALSE(utils::starts_with(str, "Teststrin"));
	EXPECT_FALSE(utils::starts_with(str, "Teststring "));
}


TEST(Utils, StartsWithString)
{
	// TODO test against / with empty string
	std::string str("Test string");

	EXPECT_TRUE(utils::starts_with(str, std::string("T")));
	EXPECT_TRUE(utils::starts_with(str, std::string("Te")));
	EXPECT_TRUE(utils::starts_with(str, std::string("Tes")));
	EXPECT_TRUE(utils::starts_with(str, std::string("Test")));
	EXPECT_TRUE(utils::starts_with(str, std::string("Test ")));
	EXPECT_TRUE(utils::starts_with(str, std::string("Test s")));
	EXPECT_TRUE(utils::starts_with(str, std::string("Test st")));
	EXPECT_TRUE(utils::starts_with(str, std::string("Test str")));
	EXPECT_TRUE(utils::starts_with(str, std::string("Test stri")));
	EXPECT_TRUE(utils::starts_with(str, std::string("Test strin")));
	EXPECT_TRUE(utils::starts_with(str, std::string("Test string")));

	EXPECT_FALSE(utils::starts_with(str, std::string(" T")));
	EXPECT_FALSE(utils::starts_with(str, std::string("te")));
	EXPECT_FALSE(utils::starts_with(str, std::string("tes ")));
	EXPECT_FALSE(utils::starts_with(str, std::string("Test-")));
	EXPECT_FALSE(utils::starts_with(str, std::string("Test\n")));
	EXPECT_FALSE(utils::starts_with(str, std::string("test s")));
	EXPECT_FALSE(utils::starts_with(str, std::string("Test St")));
	EXPECT_FALSE(utils::starts_with(str, std::string("Test stt")));
	EXPECT_FALSE(utils::starts_with(str, std::string("Test srti")));
	EXPECT_FALSE(utils::starts_with(str, std::string("Teststrin")));
	EXPECT_FALSE(utils::starts_with(str, std::string("Teststring ")));
}


TEST(Utils, EndsWithArrayOfChar)
{
	// TODO test against / with empty string
	std::string str("Test string");

	EXPECT_TRUE(utils::ends_with(str, "g"));
	EXPECT_TRUE(utils::ends_with(str, "ng"));
	EXPECT_TRUE(utils::ends_with(str, "ing"));
	EXPECT_TRUE(utils::ends_with(str, "ring"));
	EXPECT_TRUE(utils::ends_with(str, "tring"));
	EXPECT_TRUE(utils::ends_with(str, "string"));
	EXPECT_TRUE(utils::ends_with(str, " string"));
	EXPECT_TRUE(utils::ends_with(str, "t string"));
	EXPECT_TRUE(utils::ends_with(str, "st string"));
	EXPECT_TRUE(utils::ends_with(str, "est string"));
	EXPECT_TRUE(utils::ends_with(str, "Test string"));

	EXPECT_FALSE(utils::ends_with(str, "G"));
	EXPECT_FALSE(utils::ends_with(str, "gg"));
	EXPECT_FALSE(utils::ends_with(str, " ing"));
	EXPECT_FALSE(utils::ends_with(str, " ring"));
	EXPECT_FALSE(utils::ends_with(str, "dring"));
	EXPECT_FALSE(utils::ends_with(str, "\nstring"));
	EXPECT_FALSE(utils::ends_with(str, "test string"));
	EXPECT_FALSE(utils::ends_with(str, " Test string"));
}


TEST(Utils, EndsWithString)
{
	// TODO test against / with empty string
	std::string str("Test string");

	EXPECT_TRUE(utils::ends_with(str, std::string("g")));
	EXPECT_TRUE(utils::ends_with(str, std::string("ng")));
	EXPECT_TRUE(utils::ends_with(str, std::string("ing")));
	EXPECT_TRUE(utils::ends_with(str, std::string("ring")));
	EXPECT_TRUE(utils::ends_with(str, std::string("tring")));
	EXPECT_TRUE(utils::ends_with(str, std::string("string")));
	EXPECT_TRUE(utils::ends_with(str, std::string(" string")));
	EXPECT_TRUE(utils::ends_with(str, std::string("t string")));
	EXPECT_TRUE(utils::ends_with(str, std::string("st string")));
	EXPECT_TRUE(utils::ends_with(str, std::string("est string")));
	EXPECT_TRUE(utils::ends_with(str, std::string("Test string")));

	EXPECT_FALSE(utils::ends_with(str, std::string("G")));
	EXPECT_FALSE(utils::ends_with(str, std::string("gg")));
	EXPECT_FALSE(utils::ends_with(str, std::string(" ing")));
	EXPECT_FALSE(utils::ends_with(str, std::string(" ring")));
	EXPECT_FALSE(utils::ends_with(str, std::string("dring")));
	EXPECT_FALSE(utils::ends_with(str, std::string("\nstring")));
	EXPECT_FALSE(utils::ends_with(str, std::string("test string")));
	EXPECT_FALSE(utils::ends_with(str, std::string(" Test string")));
}

TEST(Utils, HostnameFromUrl)
{
	std::string url{"https://errors.cynny.com/"};
	std::string hostname = utils::hostname_from_url(url);
	EXPECT_EQ("errors.cynny.com", hostname);
}

TEST(Utils, HostnameFromWrongUrl)
{
	std::string url{"https://///errors.cynny.com/"};
	std::string hostname = utils::hostname_from_url(url);
	EXPECT_EQ("", hostname);
}


TEST(Utils, HostnameFromUrlWithpath)
{
	std::string url{"https://errors.cynny.com/pathpathapaha"};
	std::string hostname = utils::hostname_from_url(url);
	EXPECT_EQ("errors.cynny.com", hostname);
}





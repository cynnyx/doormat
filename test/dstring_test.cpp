#include <gtest/gtest.h>
#include "../src/utils/dstring.h"

TEST(dstring, immutable)
{
	auto d = dstring::make_immutable("Pippo");
	{
		dstring d1{d};
		EXPECT_TRUE(d == d1);
	}

	{
		dstring d1 = d;
		EXPECT_TRUE(d == d1);
	}

	{
		auto d1 = d.append("Pasticcio");
		EXPECT_TRUE(d == "Pippo");
		EXPECT_TRUE(d == d1);
	}
}


TEST(dstring, sizers)
{
	dstring d;
	EXPECT_TRUE(d.empty());
	EXPECT_TRUE(d.size() == 0);

	dstring d1{"asdfgh"};
	EXPECT_FALSE(d1.empty());
	EXPECT_TRUE(d1.size() == 6);

	EXPECT_EQ(std::string(d1), "asdfgh");
}

TEST(dstring, ctors)
{
	dstring d;
	EXPECT_FALSE(d);

	dstring d0{"ciaone"};

	dstring d1{d};
	EXPECT_TRUE(d1 == d);

	dstring d2{std::move(d1)};
	EXPECT_TRUE(d2 == d);

	dstring d3{"PiPpo", 5, true};
	EXPECT_EQ(std::string(d3), "pippo");

	dstring d4{"PippoPasticcio"};
	EXPECT_EQ(std::string(d4.cdata(), d4.size()), "PippoPasticcio");

	auto c = "PippoPasticcio";
	dstring d5{c, strlen(c)};
	EXPECT_EQ(std::string(d5.cdata(), d5.size()), "PippoPasticcio");

	dstring d6{(const unsigned char*)"PippoPasticcio"};
	EXPECT_EQ(std::string(d6.cdata(), d6.size()), "PippoPasticcio");

	dstring d7{(const unsigned char*)c, strlen(c)};
	EXPECT_EQ(std::string(d7.cdata(), d7.size()), "PippoPasticcio");
}

TEST(dstring, operators)
{
	dstring d{"AmbarabàCiciCocò"};
	dstring d1 = d;
	EXPECT_TRUE(d1 == d);

	dstring d2{"AmbarabàCiciCocò"};
	EXPECT_TRUE(d1 == d2);

	dstring d3{"pipp0"};
	EXPECT_TRUE(d3 != d2);

	dstring d4{"pipp0", 5, true};
	EXPECT_TRUE(d3 == d4);

	std::string tmp{"string"};
	dstring d5{"string"};
	dstring d6{"cstr"};
	EXPECT_TRUE(d5 == tmp);
	EXPECT_FALSE(d6 == tmp);
	EXPECT_TRUE(d6 == "cstr");
	EXPECT_FALSE(d5 == "cstr");
}

TEST(dstring, clone)
{
	dstring d{"AmbarabàCiciCocò"};
	auto d1 = d.clone();
	EXPECT_TRUE(d1 == d);
	EXPECT_TRUE(d1 == "AmbarabàCiciCocò");

	auto d2 = d.clone(::tolower);
	EXPECT_TRUE(d2.size() == d.size());
	EXPECT_TRUE(d2 == "ambarabàcicicocò");
}

TEST(dstring, substr)
{
	dstring d{"AmbarabaCiciCoco"};
	auto d1 = d.substr(0, 8);
	EXPECT_EQ((std::string{d1.cdata(), d1.size()}), "Ambaraba");

	auto d2 = d.substr(8, 4, ::tolower);
	EXPECT_EQ((std::string{d2.cdata(), d2.size()}), "cici");
	auto d3 = d.substr(d.cbegin(), d.cbegin() + 8);
	EXPECT_EQ((std::string{d3.cdata(), d3.size()}), "Ambaraba");
}

TEST(dstring, converters)
{
	size_t int_value;

	dstring d;
	EXPECT_EQ(bool(d), false);
	EXPECT_EQ((std::string{d.cdata(), d.size()}), "");

	dstring d1{"Pippo"};
	EXPECT_EQ(bool(d1), true);
	EXPECT_EQ((std::string{d1.cdata(), d1.size()}), "Pippo");
	EXPECT_EQ(d1.to_integer(int_value), false);

	dstring d2{" 42"};
	EXPECT_EQ(d2.to_integer(int_value), true);
	EXPECT_EQ(int_value, 42U);

	dstring d4{"pluto"};
	EXPECT_EQ(std::string(d4), "pluto");
}


TEST(dstring, iterators)
{
	dstring d{"qwerty"};
	EXPECT_EQ(d.front(), *d.cbegin());
	auto cend = d.cend()-1;
	EXPECT_EQ(d.back(), *cend);
	size_t count{0};
	for( auto it = d.cbegin(); it < d.cend(); ++it )
		count++;
	EXPECT_EQ(d.size(), count);
}

TEST(dstring, find)
{
	dstring d{"abcd:efgh"};
	EXPECT_EQ(d.find(';'), dstring::npos);
	EXPECT_EQ(d.find(':'), 4U);
}

TEST(dstring, append)
{
	dstring d1("abcd");
	EXPECT_EQ(d1.size(), 4U);

	d1.append("efgh",4);
	EXPECT_EQ(d1.size(), 8U);

	dstring d2 = d1;
	EXPECT_TRUE(d2 == d1);
	EXPECT_TRUE(d2.cdata() == d1.cdata());

	d1.append("ilmn",4);
	EXPECT_EQ(d1.size(), 12U);
	EXPECT_FALSE(d2 == d1);
	EXPECT_FALSE(d2.cdata() == d1.cdata());

	EXPECT_EQ(std::string(d2),"abcdefgh");
	EXPECT_EQ(std::string(d1),"abcdefghilmn");
}

TEST(dstring, append_immutable)
{
	auto d1 = dstring::make_immutable("abcd");
	EXPECT_EQ(d1.size(), 4U);

	dstring d2 = d1;
	EXPECT_TRUE(d2 == d1);
	EXPECT_TRUE(d2.cdata() == d1.cdata());

	d2.append("ilmn",4);
	EXPECT_FALSE(d2 == d1);
	EXPECT_FALSE(d2.cdata() == d1.cdata());

	EXPECT_EQ(std::string(d2),"abcdilmn");
}

TEST(dstring, append_insensitive)
{
	dstring d1{"aBcD",4, true};
	EXPECT_EQ(d1.size(), 4U);

	d1.append("efGh",4);
	EXPECT_EQ(d1.size(), 8U);

	EXPECT_EQ(std::string(d1),"abcdefgh");
}

TEST(dstring, reset)
{
	dstring d1("abcd");
	EXPECT_EQ(d1.size(), 4U);

	//Limited scope
	{ dstring{d1};}

	EXPECT_EQ(std::string(d1),"abcd");
}

TEST(dstring, empty)
{
	dstring dx;
	std::string boomer = static_cast<std::string>(dx);
	EXPECT_FALSE(static_cast<bool>( dx ));
	EXPECT_TRUE(dx.empty());
	EXPECT_EQ( boomer, static_cast<std::string>(dx));
	EXPECT_EQ( static_cast<std::string>(dx), "");
}

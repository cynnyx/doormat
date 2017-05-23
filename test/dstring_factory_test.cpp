#include <gtest/gtest.h>
#include "../src/utils/dstring.h"
#include "../src/utils/dstring_factory.h"

TEST(dstring_factory, usage)
{
	dstring_factory dsf{5};
	EXPECT_EQ(dsf.max_size(), 5U);

	memcpy(dsf.data(),"pippo", 5);
	auto d = dsf.create_dstring(5);

	EXPECT_TRUE(d.is_valid());
	EXPECT_EQ(d.size(),5U);
	EXPECT_EQ(std::string(d), "pippo");
}

TEST(dstring_factory, create_invalid)
{
	dstring_factory dsf{10};
	EXPECT_EQ(dsf.max_size(), 10U);

	auto d = dsf.create_dstring();
	EXPECT_FALSE(d.is_valid());
	EXPECT_FALSE(d.cdata());
	EXPECT_EQ(d.size(), 0U);
}

TEST(dstring_factory, create_twice)
{
	dstring_factory dsf{10};
	EXPECT_EQ(dsf.max_size(), 10U);

	memcpy(dsf.data(),"pippo", 5);
	auto d1 = dsf.create_dstring(5);

	EXPECT_TRUE(d1.is_valid());
	EXPECT_EQ(d1.size(),5U);
	EXPECT_EQ(std::string(d1), "pippo");

	memcpy(dsf.data(),"pippo", 5);
	auto d2 = dsf.create_dstring(5);

	EXPECT_TRUE(d2.is_valid());
	EXPECT_EQ(d2.size(),5U);
	EXPECT_EQ(std::string(d2), "pippo");

	EXPECT_TRUE(d1 == d2);
	EXPECT_FALSE( d1.cdata() == d2.cdata());
}

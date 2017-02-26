#include <gtest/gtest.h>
#include "../src/utils/dstring.h"
#include "../src/utils/dstring_factory.h"

TEST(dstring_factory, usage)
{
	dstring_factory dsf{5};
	EXPECT_EQ(dsf.max_size(), 5);

	memcpy(dsf.data(),"pippo", 5);
	auto d = dsf.create_dstring(5);

	EXPECT_TRUE(d);
	EXPECT_EQ(d.size(),5);
	EXPECT_EQ(std::string(d), "pippo");
}

TEST(dstring_factory, create_invalid)
{
	dstring_factory dsf{10};
	EXPECT_EQ(dsf.max_size(), 10);

	auto d = dsf.create_dstring();
	EXPECT_FALSE(d);
	EXPECT_FALSE(d.cdata());
	EXPECT_EQ(d.size(),0);
}

TEST(dstring_factory, create_twice)
{
	dstring_factory dsf{10};
	EXPECT_EQ(dsf.max_size(), 10);

	memcpy(dsf.data(),"pippo", 5);
	auto d1 = dsf.create_dstring(5);

	EXPECT_TRUE(d1);
	EXPECT_EQ(d1.size(),5);
	EXPECT_EQ(std::string(d1), "pippo");

	memcpy(dsf.data(),"pippo", 5);
	auto d2 = dsf.create_dstring(5);

	EXPECT_TRUE(d2);
	EXPECT_EQ(d2.size(),5);
	EXPECT_EQ(std::string(d2), "pippo");

	EXPECT_TRUE(d1 == d2);
	EXPECT_FALSE( d1.cdata() == d2.cdata());
}

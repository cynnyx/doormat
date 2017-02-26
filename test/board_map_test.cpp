#include "../src/board/board_map.h"
#include "../src/constants.h"
#include "nodes/common.h"
#include <string>

using namespace test_utils;
using namespace routing;
using std::string;

struct board_mock_conf : public preset::mock_conf
{
	std::string get_route_map() const noexcept override
	{
		static constexpr auto rm = "./etc/doormat/route_map2";
		return rm;
	}
};

struct fixedboardmap : public preset::test
{
	virtual void SetUp() override
	{
		preset::setup(new board_mock_conf{} );
	}
};

TEST_F(fixedboardmap,boardeq)
{
	abstract_destination_provider::address  b{"127.0.0.1",2000};
	abstract_destination_provider::address  b2{"127.0.0.1",2000};
	ASSERT_TRUE( b == b2 );
	ASSERT_FALSE( b < b2 );
}

TEST_F(fixedboardmap,loading)
{
	board_map bmap;
	ASSERT_TRUE ( bmap.boards().size() > 0 );
}

TEST_F(fixedboardmap,retrieving)
{
	board_map bmap;

	abstract_destination_provider::address ip_dest = bmap.retrieve_destination();
	EXPECT_TRUE( ip_dest.is_valid() );

	abstract_destination_provider::address ip_dest2 = bmap.retrieve_destination();
	EXPECT_TRUE( ip_dest2.is_valid() );

	abstract_destination_provider::address ip_dest3 = bmap.retrieve_destination();
	EXPECT_TRUE( ip_dest3.is_valid() );

	abstract_destination_provider::address ip_dest4 = bmap.retrieve_destination();
	EXPECT_TRUE( ip_dest4.is_valid() );
}

TEST_F(fixedboardmap,failed)
{
	board_map bmap;

	abstract_destination_provider::address ip_dest1 = bmap.retrieve_destination();
	bmap.destination_failed( ip_dest1 );
	bool done = false;

	std::vector<board> boards = bmap.boards();
	for ( board b : boards )
	{
		abstract_destination_provider::address ip{ b.ipv6_address, b.port };
		if ( ip == ip_dest1 )
		{
			done = true;
			short x = b.failed_times.load();
			EXPECT_TRUE( x == 1 );
		}
	}
	EXPECT_TRUE( done );
}

TEST_F(fixedboardmap,worked)
{
	board_map bmap;
	bool done{false};

	abstract_destination_provider::address ip_dest1 = bmap.retrieve_destination();
	bmap.destination_failed( ip_dest1 );
	bmap.destination_worked( ip_dest1 );

	std::vector<board> boards = bmap.boards();
	for ( board b : boards )
	{
		abstract_destination_provider::address ip{ b.ipv6_address, b.port };
		if ( ip == ip_dest1 )
		{
			done = true;
			short x = b.failed_times.load();
			EXPECT_TRUE( x == 0 );
		}
	}
	EXPECT_TRUE( done );
}

TEST_F(fixedboardmap, disabled)
{
	board_map bmap;
	bool done{false};

	abstract_destination_provider::address ip_dest1 = bmap.retrieve_destination();
	bmap.disable_destination( ip_dest1 );

	for ( board b : bmap.boards() )
	{
		abstract_destination_provider::address ip{ b.ipv6_address, b.port };
		if ( ip == ip_dest1 )
		{
			EXPECT_FALSE( b.enable );
		}

		ip = bmap.retrieve_destination();

		if( ip == ip_dest1 )
		{
			FAIL();
		}

		done = true;
	}

	EXPECT_TRUE( done );
}


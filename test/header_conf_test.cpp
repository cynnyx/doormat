#include <gtest/gtest.h>

#include "../src/service_locator/service_initializer.h"
#include "../src/service_locator/service_locator.h"
#include "../src/configuration/header_configuration.h"

using namespace configuration;

TEST(header_conf, load_empty)
{
	service::initializer::load_configuration("./etc/doormat/doormat.testx.config");
	service::initializer::init_services();
	const header& hs = service::locator::configuration().header_configuration();

	ASSERT_FALSE( hs.filter_exist() );
	ASSERT_EQ( hs.begin(), hs.end() );
}

TEST(header_conf, load_no_kill)
{
	service::initializer::load_configuration("./etc/doormat/doormat.test.config");
	service::initializer::init_services();
	const header& hs = service::locator::configuration().header_configuration();

	ASSERT_FALSE( hs.filter_exist() );
	ASSERT_NE( hs.begin(), hs.end() );

	int counter = 0;
	for ( auto it = hs.begin(); it != hs.end(); ++it )
		++counter;

	ASSERT_EQ(9, counter);
}

TEST(header_conf, load_kill)
{
	service::initializer::load_configuration("./etc/doormat/doormat.testx2.config");
	service::initializer::init_services();
	const header& hs = service::locator::configuration().header_configuration();

	ASSERT_TRUE( hs.filter_exist() );
}

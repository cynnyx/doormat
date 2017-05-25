#include <gtest/gtest.h>
#include "../src/service_locator/service_initializer.h"
#include "../src/service_locator/service_locator.h"
#include "../src/io_service_pool.h"
#include "src/network/communicator/communicator.h"
#include "src/network/communicator/dns_communicator_factory.h"
#include "mocks/mock_server/mock_server.h"
#include "testcommon.h"
struct dns_factory_test: public ::testing::Test
{
    class mockconf: public configuration::configuration_wrapper
    {
        uint64_t get_board_timeout() const noexcept override { return 3000; }
    };
    virtual void SetUp()
    {
        service::initializer::set_service_pool( new server::io_service_pool( 1 ) );
        service::initializer::set_configuration( new mockconf() );
    }
    virtual void TearDown()
    {

    }
};

TEST_F(dns_factory_test, connect_http)
{
	mock_server<> m;
    network::dns_connector_factory f;
	auto address = "localhost";
	auto port = 8454;
	auto tls = false;
    int count{0};
    m.start([&count](){++count; if(count == 2) service::locator::service_pool().stop(); }, true);
	f.get_connector(address, port, tls, [&count](auto s){
        ASSERT_TRUE(bool(s));
        ++count;
        if(count == 2)
            service::locator::service_pool().stop();

    }, [](auto s){ FAIL() << "Failed with error code" << s;  });
    service::locator::service_pool().run();
    ASSERT_EQ(count, 2);
}

TEST_F(dns_factory_test, fail_connect)
{
	network::dns_connector_factory f;
	auto address = "localhost";
	auto port = 8454;
	auto tls = false;
    int count{0};
	f.get_connector(address, port, tls, [&count](auto s){
        FAIL() << "Should not be able to connect to invalid port.";
    }, [&count](auto s){
        service::locator::service_pool().stop();
        ++count;
    });
    service::locator::service_pool().run();
    ASSERT_EQ(count, 1);
}

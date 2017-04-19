#include <gtest/gtest.h>
#include "../src/service_locator/service_initializer.h"
#include "../src/service_locator/service_locator.h"
#include "../src/io_service_pool.h"
#include "../src/network/communicator.h"
#include "../src/network/dns_connector_factory.h"
#include "../mock_server/mock_server.h"
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
    mock_server m;
    network::dns_connector_factory f;
    http::http_request request;
    request.setParameter("hostname", "localhost");
    request.setParameter("port", "8454");
    int count{0};
    m.start([&count](){++count; if(count == 2) service::locator::service_pool().stop(); }, true);
    f.get_connector(request, [&count](auto s){ ++count; if(count == 2) service::locator::service_pool().stop();  }, [](auto s){ FAIL() << "Failed with error code" << s;  });
    service::locator::service_pool().run();
    ASSERT_EQ(count, 2);
}
#include "gtest/gtest.h"

#include "src/http_client.h"
#include "src/network/communicator/dns_communicator_factory.h"
#include "src/service_locator/service_initializer.h"
#include "src/service_locator/service_locator.h"
#include "src/io_service_pool.h"

#include "mocks/mock_server/mock_server.h"

struct http_client_test: public ::testing::Test
{
	class mockconf: public configuration::configuration_wrapper
	{
		uint64_t get_board_timeout() const noexcept override { return 3000; }
	};
	virtual void SetUp()
	{
		service::initializer::set_configuration( new mockconf() );
	}


	boost::asio::io_service io;
};

using http_client = client::http_client<network::dns_connector_factory>;

auto port = 8454U;


TEST_F(http_client_test, connect_ipv4_clear)
{
	mock_server<> server{io};
	server.start([]{});
	bool succeded{false};
	http_client client{io};
	client.connect([&server, &succeded](auto connection)
	{
		SUCCEED();
		succeded = true;
	//	service::locator::service_pool().allow_graceful_termination();
		server.stop();
	},
	[](auto error)
	{
		FAIL();
	}, http::proto_version::HTTP11, "127.0.0.1", port, false);

	io.run();
	ASSERT_TRUE(succeded);
}


TEST_F(http_client_test, connect_ipv4_ssl)
{
	mock_server<true> server{io};
	server.start([]{});
	bool succeded{false};
	http_client client{io};
	client.connect([&server, &succeded](auto connection)
	{
		succeded = true;
		SUCCEED();
		server.stop();
	},
	[](auto error)
	{
		FAIL();
	}, http::proto_version::HTTP11, "127.0.0.1", port, true);

	io.run();
	ASSERT_TRUE(succeded); 
}
/*

TEST_F(http_client_test, connect_ipv6_clear)
{
	mock_server<> server;
	server.start([]{});

	http_client client;
	client.connect([&server](auto connection)
	{
		SUCCEED();
		service::locator::service_pool().allow_graceful_termination();
		server.stop();
	},
	[](auto error)
	{
		FAIL();
	}, http::proto_version::HTTP11, "::1", port, false);

	service::locator::service_pool().run();
}


TEST_F(http_client_test, connect_ipv6_ssl)
{
	mock_server<true> server;
	server.start([]{});

	http_client client;
	client.connect([&server](auto connection)
	{
		SUCCEED();
		service::locator::service_pool().allow_graceful_termination();
		server.stop();
	},
	[](auto error)
	{
		FAIL();
	}, http::proto_version::HTTP11, "::1", port, true);

	service::locator::service_pool().run();
}


TEST_F(http_client_test, fail_connect)
{
	mock_server<true> server;
	server.start([]{});

	http_client client;
	client.connect([](auto connection)
	{
		FAIL();
	},
	[&server](auto error)
	{
		SUCCEED();
		service::locator::service_pool().allow_graceful_termination();
		server.stop();
	}, http::proto_version::HTTP11, "::1", port + 1, true);

	service::locator::service_pool().run();
}*/

#include "gtest/gtest.h"

#include "src/http_client.h"
#include "src/network/communicator/dns_communicator_factory.h"

#include "mocks/mock_server/mock_server.h"

struct http_client_test: public ::testing::Test
{
	boost::asio::io_service io;
};

using http_client = client::http_client<network::dns_connector_factory>;

const auto port = 8454U;
const auto timeout = std::chrono::milliseconds{100};


TEST_F(http_client_test, connect_ipv4_clear)
{
	mock_server<> server{io};
	server.start([]{});

	bool succeeded{false};
	http_client client{io, timeout};
	client.connect([&server, &succeeded](auto connection) {
		               SUCCEED();
		               succeeded = true;
		               server.stop();
	               },
	               [](auto error) {
		               FAIL();
	               },
	               http::proto_version::HTTP11, "127.0.0.1", port, false);

	io.run();
	ASSERT_TRUE(succeeded);
}


TEST_F(http_client_test, connect_ipv4_ssl)
{
	mock_server<true> server{io};
	server.start([]{});

	bool succeeded{false};
	http_client client{io, timeout};
	client.connect([&server, &succeeded](auto connection)
	{
		succeeded = true;
		SUCCEED();
		server.stop();
	},
	[](auto error)
	{
		FAIL();
	}, http::proto_version::HTTP11, "127.0.0.1", port, true);

	io.run();
	ASSERT_TRUE(succeeded);
}


TEST_F(http_client_test, connect_ipv6_clear)
{
	mock_server<> server{io};
	server.start([]{});

	bool succeeded{false};
	http_client client{io, timeout};
	client.connect([&server, &succeeded](auto connection)
	{
		succeeded = true;
		SUCCEED();
		server.stop();
	},
	[](auto error)
	{
		FAIL();
	}, http::proto_version::HTTP11, "::1", port, false);

	io.run();
	ASSERT_TRUE(succeeded);
}


TEST_F(http_client_test, connect_ipv6_ssl)
{
	mock_server<true> server{io};
	server.start([]{});

	bool succeeded{false};
	http_client client{io, timeout};
	client.connect([&server, &succeeded](auto connection)
	{
		succeeded = true;
		SUCCEED();
		server.stop();
	},
	[](auto error)
	{
		FAIL();
	}, http::proto_version::HTTP11, "::1", port, true);

	io.run();
	ASSERT_TRUE(succeeded);
}


TEST_F(http_client_test, fail_connect)
{
	mock_server<true> server{io};
	server.start([]{});

	bool called {false};
	http_client client{io, timeout};
	client.connect([](auto connection)
	{
		FAIL();
	},
	[&server, &called](auto error)
	{
		SUCCEED();
		called = true;
		server.stop();
	}, http::proto_version::HTTP11, "::1", port + 1, true);

	io.run();
	ASSERT_TRUE(called);
}

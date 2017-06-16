#include "gtest/gtest.h"

#include "src/http_client.h"
#include "src/network/communicator/dns_communicator_factory.h"
#include "src/protocol/handler_http1.h"
#include "src/http2/session_client.h"

#include "mocks/mock_server/mock_server.h"

struct http_client_test: public ::testing::Test
{
	boost::asio::io_service io;
};

using http_client = client::http_client<network::dns_connector_factory>;
using handler_http1 = server::handler_http1<http::client_traits>;

using http_client2 = client::http_client<client::detail::handler_factory<network::dns_connector_factory>>;
const auto port = 8454U;
const auto timeout = std::chrono::milliseconds{100};


TEST_F(http_client_test, connect_ipv4_clear)
{
	mock_server<> server{io};
	server.start([]{});

	bool succeeded{false};
	http_client client{io, timeout};
	client.connect([&server, &succeeded](auto connection) {
					   // check that clear connections go with http1.x proto
					   EXPECT_TRUE(std::dynamic_pointer_cast<handler_http1>(connection));
					   succeeded = true;
					   server.stop();
				   },
				   [](auto error) {
					   FAIL();
				   }, "127.0.0.1", port, false);

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
		// check that ssl connections go with http2 proto
		EXPECT_TRUE(std::dynamic_pointer_cast<http2::session_client>(connection));
		server.stop();
	},
	[](auto error)
	{
		FAIL();
	}, "127.0.0.1", port, true);

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
		// check that clear connections go with http1.x proto
		EXPECT_TRUE(std::dynamic_pointer_cast<handler_http1>(connection));
		server.stop();
	},
	[](auto error)
	{
		FAIL();
	}, "::1", port, false);

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
		// check that ssl connections go with http2 proto
		EXPECT_TRUE(std::dynamic_pointer_cast<http2::session_client>(connection));
		server.stop();
	},
	[](auto error)
	{
		FAIL();
	}, "::1", port, true);

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
	},"::1", port + 1, true);

	io.run();
	ASSERT_TRUE(called);
}


TEST_F(http_client_test, connect_ipv4_clear2)
{
	mock_server<> server{io};
	server.start([]{});

	bool succeeded{false};
	http_client2 client{io, timeout};
	client.connect([&server, &succeeded](auto connection) {
		               SUCCEED();
		               succeeded = true;
		               server.stop();
	               },
	               [](auto error) {
		               FAIL();
	               },
	               "127.0.0.1", port, false);

	io.run();
	ASSERT_TRUE(succeeded);
}



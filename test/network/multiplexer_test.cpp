#include <gtest/gtest.h>
#include "../../../src/http_client.h"
#include "../../../src/http/client/client_connection.h"
#include "../../../src/http/client/client_traits.h"
#include "../../../src/protocol/handler_http1.h"
#include "../../../src/http/client/request.h"
#include "../../../src/http/client/response.h"
#include "../../../src/network/client_connection_multiplexer.h"
#include "../../../src/network/communicator/dns_communicator_factory.h"

#include "../mocks/mock_server/mock_server.h"

using http_client = client::http_client<network::dns_connector_factory>;
const auto port = 8454U;
const auto timeout = std::chrono::milliseconds{100};

TEST(multiplexer, single_timeout)
{
	boost::asio::io_service io;

	mock_server<> server{io};
	server.start([]{});

	bool succeeded{false};
	http_client client{io, timeout};
	std::shared_ptr<network::client_connection_multiplexer> multi{nullptr};
	client.connect([&server, &succeeded, &multi](auto connection)
		{
			multi = std::make_shared<network::client_connection_multiplexer>(std::move(connection));
			multi->init();
			auto handler = multi->get_handler();
			handler->on_timeout(std::chrono::milliseconds{500U}, [handler, &server, &succeeded, &multi](auto conn){

				SUCCEED();
				succeeded = true;
				handler->on_timeout(std::chrono::milliseconds{500U}, [](auto conn){});
				server.stop();
				conn->close();
			});
		},
		[](auto error) {
			FAIL();
		}, http::proto_version::HTTP11, "127.0.0.1", port, false);

	io.run();
	ASSERT_TRUE(succeeded);
}


TEST(multiplexer, multiple_errors)
{
	boost::asio::io_service io;

	mock_server<> server{io};
	server.start([]{});
	boost::asio::deadline_timer t{io};
	t.expires_from_now(boost::posix_time::milliseconds(100));
	t.async_wait([&server](auto &errc){
		server.stop();
	});
	size_t errors{0};
	http_client client{io, timeout};
	std::shared_ptr<network::client_connection_multiplexer> multi{nullptr};
	client.connect([&server, &errors, &multi](auto connection)
	               {
		               multi = std::make_shared<network::client_connection_multiplexer>(std::move(connection));
		               multi->init();
		               auto handler = multi->get_handler();
		               handler->on_error([&errors, handler](auto conn, auto &err)
		                                 {
			                                 ++errors;
			                                 handler->on_error([](auto conn, auto&err){});
		                                 });
		               auto handler2 = multi->get_handler();
		               handler2->on_error([&errors, handler2](auto conn, auto &err)
		                                  {
			                                  ++errors;
			                                  handler2->on_error([](auto conn, auto&err){});
		                                  });
	               },
	               [](auto error) {
		               FAIL();
	               }, http::proto_version::HTTP11, "127.0.0.1", port, false);

	io.run();
	ASSERT_EQ(errors, 2U);
}
TEST(multiplexer, multiple_timeout)
{
	boost::asio::io_service io;

	mock_server<> server{io};
	server.start([]{});

	size_t timeouts{0};
	bool error_received{false};
	http_client client{io, timeout};
	std::shared_ptr<network::client_connection_multiplexer> multi{nullptr};
	client.connect([&server, &timeouts, &multi, &error_received](auto connection)
	               {
		               multi = std::make_shared<network::client_connection_multiplexer>(std::move(connection));
		               multi->init();
		               auto handler = multi->get_handler();
		               handler->on_timeout(std::chrono::milliseconds{500U}, [handler, &timeouts, &multi](auto conn){
			               ++timeouts;
		               });
		               handler->on_error([&error_received, handler](auto conn, auto err){
			               error_received = true;
			               handler->on_timeout(std::chrono::milliseconds{100U}, [](auto c){});
			               handler->on_error([](auto c, auto &err){});
		               });
		               auto handler2 = multi->get_handler();

		               handler2->on_timeout(std::chrono::milliseconds{1200U},  [handler2, &server, &timeouts](auto conn){
			               ASSERT_EQ(timeouts, 2U);
			               conn->close();
			               server.stop();
			               ++timeouts;
			               handler2->on_timeout(std::chrono::milliseconds{100}, [](auto conn){});
	                   });
	               },
	               [](auto error) {
		               FAIL();
	               }, http::proto_version::HTTP11, "127.0.0.1", port, false);

	io.run();
	ASSERT_EQ(timeouts, 3U);
	ASSERT_TRUE(error_received);
}
#include <gtest/gtest.h>
#include "../../../src/http_client.h"
#include "../../../src/http/client/client_connection.h"
#include "../../../src/http/client/client_traits.h"
#include "../../../src/protocol/handler_http1.h"
#include "../../../src/http/client/request.h"
#include "../../../src/http/client/response.h"
#include "../../../src/http/client/client_connection_multiplexer.h"
#include "../../../src/network/communicator/dns_communicator_factory.h"
#include "../testcommon.h"
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
	std::shared_ptr<http::client_connection_multiplexer> multi{nullptr};
	client.connect([&server, &succeeded, &multi](auto connection)
		{
			multi = std::make_shared<http::client_connection_multiplexer>(std::move(connection));
			multi->init();
			auto handler = multi->get_handler();
			handler->on_timeout(std::chrono::milliseconds{500U}, [handler, &server, &succeeded, &multi](auto conn){
				std::cout << "is this std::function invalid?" << std::endl;
				SUCCEED();
				succeeded = true;
				conn->close();
				handler->on_timeout(std::chrono::milliseconds{500U}, [&server](auto conn){
					//server.stop();
				});

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
	std::shared_ptr<http::client_connection_multiplexer> multi{nullptr};
	client.connect([&server, &errors, &multi](auto connection)
	               {
		               multi = std::make_shared<http::client_connection_multiplexer>(std::move(connection));
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
	std::shared_ptr<http::client_connection_multiplexer> multi{nullptr};
	client.connect([&server, &timeouts, &multi, &error_received](auto connection)
	               {
		               multi = std::make_shared<http::client_connection_multiplexer>(std::move(connection));
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


TEST(multiplexer, single_req_res)
{
	boost::asio::io_service io;

	mock_server<> server{io};
	std::string readed{};
	size_t request_count{0};
	auto expected_req = make_request(http::proto_version::HTTP11, http_method::HTTP_GET, "prova.com", "/ciao");
	std::function<void(std::string)> read_callback;
	read_callback = [&readed, &server, &read_callback, expected_req = std::move(expected_req), &request_count](std::string bytes){
		readed += bytes;
		if(expected_req.serialize() == readed)
		{
			const std::string expected_response = "HTTP/1.1 200 OK\r\n"
					                                      "connection: keep-alive\r\n"
					                                      "content-length: 33\r\n"
					                                      "content-type: text/plain\r\n"
					                                      "date: Tue, 17 May 2016 14:53:09 GMT\r\n"
					                                      "request-count: "+ std::to_string(request_count) + "\r\n"
					                                      "\r\n"
					                                      "Ave client, dummy node says hello";
			++request_count;
			server.write(expected_response);
			readed = "";
		}
		server.read(1, read_callback);

	};
	server.start([&read_callback, &server]{
		server.read(1, read_callback);
	});


	http_client client{io, timeout};
	std::shared_ptr<http::client_connection_multiplexer> multi{nullptr};
	size_t headers_rcvd{0};
	size_t body_rcvd{0};
	size_t finished_rcvd{0};
	client.connect([&server, &headers_rcvd, &body_rcvd, &finished_rcvd, &multi](auto connection)
	               {
		               std::cout << "c: " << connection.use_count() << std::endl;
		               multi = std::make_shared<http::client_connection_multiplexer>(connection);
		               multi->init();
		               std::cout << "c: " << connection.use_count() << std::endl;
		               for(size_t i = 0; i < 1U; ++i)
		               {
			               auto handler = multi->get_handler();
			               std::cout << "connection use count?" << connection.use_count() << std::endl;
			               auto transaction = handler->create_transaction();

			               auto &request = transaction.first;
			               auto &response = transaction.second;
			               response->on_headers([handler, &headers_rcvd, i](auto resp){
				               ASSERT_EQ(headers_rcvd, i);
				               ASSERT_TRUE(resp->preamble().has("request-count"));
				               ASSERT_EQ(resp->preamble().header("request-count"), std::to_string(i));
				               headers_rcvd++;
			               });
			               response->on_body([handler, &headers_rcvd, &body_rcvd, i](auto resp, auto b, auto s){
				               ASSERT_EQ(body_rcvd, i);
				               body_rcvd++;
			               });
			               response->on_finished([handler, &server, &headers_rcvd, &body_rcvd, &finished_rcvd, i, &multi, connection](auto resp){
				               std::cout << "connection use count is" << connection.use_count() << std::endl;
				               ASSERT_EQ(finished_rcvd, i);
				               finished_rcvd++;
				               resp->get_connection()->close();
				               server.stop();
				               multi = nullptr;
			               });

			               request->headers(make_request(http::proto_version::HTTP11, http_method::HTTP_GET, "prova.com", "/ciao"));
			               request->end();
		               }
	               },
	               [](auto error) {
		               FAIL();
	               }, http::proto_version::HTTP11, "127.0.0.1", port, false);

	io.run();
	std::cout << "multi is " << multi << std::endl;
	ASSERT_EQ(headers_rcvd, 1U);
	ASSERT_EQ(body_rcvd, 1U);
	ASSERT_EQ(finished_rcvd, 1U);
}


TEST(multiplexer, multiple_req_res)
{
	boost::asio::io_service io;

	mock_server<> server{io};
	std::string readed{};
	size_t request_count{0};
	auto expected_req = make_request(http::proto_version::HTTP11, http_method::HTTP_GET, "prova.com", "/ciao");
	std::function<void(std::string)> read_callback;
	read_callback = [&readed, &server, &read_callback, expected_req = std::move(expected_req), &request_count](std::string bytes){
		readed += bytes;
		if(expected_req.serialize() == readed)
		{
			const std::string expected_response = "HTTP/1.1 200 OK\r\n"
					"connection: keep-alive\r\n"
					"content-length: 33\r\n"
					"content-type: text/plain\r\n"
					"date: Tue, 17 May 2016 14:53:09 GMT\r\n"
					"request-count: "+ std::to_string(request_count) + "\r\n"
					"\r\n"
					"Ave client, dummy node says hello";
			++request_count;
			server.write(expected_response);
			readed = "";
		}
		server.read(1, read_callback);

	};
	server.start([&read_callback, &server]{
		server.read(1, read_callback);
	});


	http_client client{io, timeout};
	std::shared_ptr<http::client_connection_multiplexer> multi{nullptr};
	size_t headers_rcvd{0};
	size_t body_rcvd{0};
	size_t finished_rcvd{0};
	client.connect([&server, &headers_rcvd, &body_rcvd, &finished_rcvd, &multi](auto connection)
	               {
		               multi = std::make_shared<http::client_connection_multiplexer>(std::move(connection));
		               multi->init();
		               for(int i = 0; i < 10; ++i)
		               {
			               auto handler = multi->get_handler();
			               auto transaction = handler->create_transaction();
			               auto &request = transaction.first;
			               auto &response = transaction.second;
			               response->on_headers([handler, &headers_rcvd, i](auto resp){
				               ASSERT_EQ(headers_rcvd, i);
				               ASSERT_TRUE(resp->preamble().has("request-count"));
				               ASSERT_EQ(resp->preamble().header("request-count"), std::to_string(i));
				               headers_rcvd++;
			               });
			               response->on_body([handler, &headers_rcvd, &body_rcvd, i](auto resp, auto b, auto s){
				               ASSERT_EQ(body_rcvd, i);
				               body_rcvd++;
			               });
			               response->on_finished([handler, &server, &headers_rcvd, &body_rcvd, &finished_rcvd, i](auto resp){
				               ASSERT_EQ(finished_rcvd, i);
				               finished_rcvd++;
				               if(i != 9) return;
				               resp->get_connection()->close();
				               server.stop();
			               });
			               request->headers(make_request(http::proto_version::HTTP11, http_method::HTTP_GET, "prova.com", "/ciao"));
			               request->end();
		               }
	               },
	               [](auto error) {
		               FAIL();
	               }, http::proto_version::HTTP11, "127.0.0.1", port, false);

	io.run();
	ASSERT_EQ(headers_rcvd, 10);
	ASSERT_EQ(body_rcvd, 10);
	ASSERT_EQ(finished_rcvd, 10);
}


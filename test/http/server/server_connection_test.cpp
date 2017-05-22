#include "../src/http/server/server_connection.h"
#include "../src/protocol/handler_http1.h"
#include "../src/http/server/server_traits.h"
#include "mocks/mock_connector.h"
#include <gtest/gtest.h>
#include <gmock/gmock.h>

using namespace http;

TEST(server_connection, invalid_request) {
	boost::asio::io_service io;
	MockConnector::wcb cb = [](dstring chunk){};
	auto handler_test = std::make_shared<server::handler_http1<http::server_traits>>(http::proto_version::HTTP11);
	auto connector = std::make_shared<MockConnector>(io, cb);
	connector->handler(handler_test);

	bool called{false};
	handler_test->on_request([&called](auto conn, auto req, auto resp) {
		req->on_error([&called](auto conn, auto &error){
			ASSERT_TRUE(conn);
			ASSERT_EQ(error.errc(), http::error_code::decoding);
			called = true;
		});
	});

	connector->io_service().post([connector](){ connector->read("GET /prova HTTP_WRONG!!!/1.1"); });
	connector->io_service().run();

	ASSERT_TRUE(called);
}


TEST(server_connection, valid_request) {
	boost::asio::io_service io;

	MockConnector::wcb cb = [](dstring chunk){};
	auto handler_test = std::make_shared<server::handler_http1<http::server_traits>>(http::proto_version::HTTP11);
	auto connector = std::make_shared<MockConnector>(io, cb);
	connector->handler(handler_test);
	size_t expected_cb_counter{2};

	size_t cb_counter{0};
	handler_test->on_request([&cb_counter](auto conn, auto req, auto resp) {
		req->on_headers([&cb_counter](auto req){
			++cb_counter;
		});

		req->on_finished([&cb_counter](auto req){
			++cb_counter;
		});
	});

	connector->io_service().post([connector](){ connector->read("GET / HTTP/1.1\r\n"
																"host:localhost:1443\r\n"
																"date: Tue, 17 May 2016 14:53:09 GMT\r\n"
																"\r\n"); });
	connector->io_service().run();
	ASSERT_EQ(cb_counter, expected_cb_counter);
}


TEST(server_connection, on_connector_nulled) {
	boost::asio::io_service io;
	MockConnector::wcb cb = [](dstring chunk){};
	auto handler_test = std::make_shared<server::handler_http1<http::server_traits>>(http::proto_version::HTTP11);
	auto connector = std::make_shared<MockConnector>(io, cb);
	connector->handler(handler_test);

	size_t expected_req_cb_counter{2};
	size_t expected_error_cb_counter{1};

	size_t req_cb_counter{0};
	size_t error_cb_counter{0};

	handler_test->on_request([&req_cb_counter, &connector, &error_cb_counter](auto conn, auto req, auto resp) {
		req->on_headers([&req_cb_counter](auto req){
			++req_cb_counter;
		});

		req->on_finished([&req_cb_counter, &conn, &connector, resp](auto req){
			++req_cb_counter;
			connector = nullptr;
			resp->headers(http::http_response{});
		});

		conn->on_error([&error_cb_counter](auto conn, const http::connection_error &error){
			ASSERT_TRUE(conn);
			ASSERT_EQ(error.errc(), http::error_code::closed_by_client);
			std::cout << "conn on error" << std::endl;
			++error_cb_counter;
		});
	});

	connector->io_service().post([connector](){
		connector->read("GET / HTTP/1.1\r\n"
						"host:localhost:1443\r\n"
						"date: Tue, 17 May 2016 14:53:09 GMT\r\n"
						"\r\n");
	});

	connector->io_service().run();
	ASSERT_EQ(req_cb_counter, expected_req_cb_counter);
	ASSERT_EQ(error_cb_counter, expected_error_cb_counter);

}


TEST(server_connection, response_continue) {
	boost::asio::io_service io;

	std::string accumulator{};
	bool rcvd{false};
	MockConnector::wcb cb = [&accumulator, &rcvd](dstring chunk){
		accumulator += std::string(chunk);
		if(accumulator == "HTTP/1.1 100 Continue\r\ncontent-length: 0\r\n\r\n") {
			rcvd = true;
		}
	};
	auto handler_test = std::make_shared<server::handler_http1<http::server_traits>>(http::proto_version::HTTP11);
	auto connector = std::make_shared<MockConnector>(io, cb);
	connector->handler(handler_test);

	handler_test->on_request([](auto conn, auto req, auto resp) {
		req->on_headers([resp](auto req){
			resp->send_continue();
		});

	});

	connector->io_service().post([connector](){ connector->read("GET / HTTP/1.1\r\n"
			                                                            "host:localhost:1443\r\n"
			                                                            "date: Tue, 17 May 2016 14:53:09 GMT\r\n"
			                                                            "\r\n"); });
	connector->io_service().run();
	ASSERT_TRUE(rcvd);
}



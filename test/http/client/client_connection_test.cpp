#include <gtest/gtest.h>
#include <boost/asio.hpp>

#include "../../../src/http/client/client_connection.h"
#include "../../../src/http/client/client_traits.h"
#include "../../../src/protocol/handler_http1.h"
#include "../../../src/http/client/request.h"
#include "../../../src/http/client/response.h"
#include "../../mocks/mock_connector.h"

using client_connection_t = server::handler_http1<http::client_traits>;

http::http_request make_dumb_request() {
	static http::http_request preamble;
	preamble.protocol(http::proto_version::HTTP11);
	preamble.urihost("localhost");
	preamble.hostname("localhost:1443");
	preamble.path("/prova");
	return preamble;
}

std::string get_expected(http::http_request &req, const std::string &body){
	if(body.size())req.content_len(body.size());
	std::string ret = std::string(req.serialize());
	if(body.size()) ret += body;
	return ret;
}


TEST(client_connection_test, send_request)
{
    boost::asio::io_service io;

	auto tested_object = std::make_shared<client_connection_t>(http::proto_version::HTTP11);
	//now register a mock connector; than go on.

	std::string body{"ciao"};
	http::http_request preamble = make_dumb_request();
	std::string received_data{};
	std::string expected_request = get_expected(preamble, body);

	MockConnector::wcb write_callback = [&](dstring d){
		received_data.append(std::string(d));
	};
    auto mock = std::make_shared<MockConnector>(io, write_callback);
	mock->handler(tested_object);

	auto user_handlers = tested_object->get_user_handlers();
	auto req = user_handlers.second;

	req->headers(std::move(preamble));
	req->body("ciao");
	req->end();
	mock->io_service().run();
	ASSERT_EQ(expected_request, received_data);
}

TEST(client_connection, request_sent_event)
{
    boost::asio::io_service io;

	auto tested_object = std::make_shared<client_connection_t>(http::proto_version::HTTP11);
	bool called;
	tested_object->on_request_sent([&called](auto conn){
		called = true;
	});

	http::http_request preamble = make_dumb_request();
	MockConnector::wcb write_callback = [](dstring d){};
    auto mock = std::make_shared<MockConnector>(io, write_callback);
	mock->handler(tested_object);
	auto user_handlers = tested_object->get_user_handlers();
	auto req = user_handlers.second;
	req->headers(std::move(preamble));
	req->end();
	mock->io_service().run();
	ASSERT_TRUE(called);
}

TEST(client_connection, read_before_write_failure)
{
//    boost::asio::io_service io;

//	auto tested_object = std::make_shared<client_connection_t>(http::proto_version::HTTP11);
//	bool called;
//	tested_object->on_error([&called](auto conn, const http::connection_error &ec){
//		ASSERT_EQ(ec.errc(), http::error_code::invalid_read);
//		called = true;
//	});
//	MockConnector::wcb write_callback = [](dstring){};
//    auto mock = std::make_shared<MockConnector>(io, write_callback);
//	mock->handler(tested_object);
//	std::string unexpected_response = "HTTP/1.1 200 OK\r\n"
//		"connection: keep-alive\r\n"
//		"content-length: 33\r\n"
//		"content-type: text/plain\r\n"
//		"date: Tue, 17 May 2016 14:53:09 GMT\r\n"
//		"\r\n"
//		"Ave client, dummy node says hello";
//	mock->read(unexpected_response);
//	mock->io_service().run();
//	ASSERT_TRUE(called);
}


TEST(client_connection, pingpong)
{
    boost::asio::io_service io;


	auto tested_object = std::make_shared<client_connection_t>(http::proto_version::HTTP11);
	//now register a mock connector; than go on.

	std::string body{"ciao"};
	http::http_request preamble = make_dumb_request();
	std::string received_data{};
	std::string expected_request = get_expected(preamble, body);

	MockConnector::wcb write_callback;
    auto mock = std::make_shared<MockConnector>(io, write_callback);
	bool finished_request{false};
	int  response_events{0};
	write_callback = [&, mock](dstring d)
		{
			received_data.append(std::string(d));
			if(received_data == expected_request && !finished_request)
			{
				std::string expected_response = "HTTP/1.1 200 OK\r\n"
						"connection: keep-alive\r\n"
						"content-length: 33\r\n"
						"content-type: text/plain\r\n"
						"date: Tue, 17 May 2016 14:53:09 GMT\r\n"
						"\r\n"
						"Ave client, dummy node says hello";
				finished_request = true;
				mock->read(expected_response);
			}
		};
	mock->handler(tested_object);

	auto user_handlers = tested_object->get_user_handlers();
	auto req = user_handlers.second;
	auto res = user_handlers.first;

	res->on_headers([&](auto response){
		ASSERT_EQ(response->preamble().status_code(), 200);
		++response_events;
	});

	res->on_body([&](auto r, std::unique_ptr<char> body, size_t s){
		std::string rcvd{body.get(), s};
		ASSERT_EQ(rcvd, "Ave client, dummy node says hello");
		++response_events;
	});

	res->on_finished([&](auto response){
		++response_events;
	});

	req->headers(std::move(preamble));
	req->body("ciao");
	req->end();
	mock->io_service().run();
	ASSERT_EQ(expected_request, received_data);
	ASSERT_TRUE(finished_request && response_events == 3);
}

#include <gtest/gtest.h>
#include <boost/asio.hpp>

#include "../../../src/http/client/client_connection.h"
#include "../../../src/http/client/client_traits.h"
#include "../../../src/protocol/handler_http1.h"
#include "../../../src/http/client/request.h"
#include "../../../src/http/client/response.h"
#include "mocks/mock_connector/mock_connector.h"

using client_connection_t = server::handler_http1<http::client_traits>;

// todo: why do we need this copy?
static std::unique_ptr<char[]> make_data_ptr(const std::string& s)
{
	auto ptr = std::make_unique<char[]>(s.size());
	std::copy(s.begin(), s.end(), ptr.get());
	return ptr;
}


http::http_request make_dumb_request() {
	http::http_request preamble;
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


TEST(client_connection, send_request)
{
    boost::asio::io_service io;
	auto tested_object = std::make_shared<client_connection_t>();
	//now register a mock connector; than go on.

	std::string body{"ciao"};
	http::http_request preamble = make_dumb_request();
	std::string received_data{};
	std::string expected_request = get_expected(preamble, body);

	MockConnector::wcb write_callback = [&](std::string d){
		received_data.append(std::string(d));
	};
    auto mock = std::make_shared<MockConnector>(io, write_callback);
	mock->handler(tested_object);

	auto user_handlers = tested_object->get_user_handlers();
	auto &req = user_handlers.second;
	auto &res = user_handlers.first;
	req->headers(std::move(preamble));
	req->body(make_data_ptr("ciao"), 4);
	req->end();
	//res->abort();
	res->get_connection()->close();
	mock->io_service().run();
	ASSERT_EQ(expected_request, received_data);
}

TEST(client_connection, request_sent_event)
{
    boost::asio::io_service io;

	auto tested_object = std::make_shared<client_connection_t>();
	bool called;
	http::http_request preamble = make_dumb_request();
	MockConnector::wcb write_callback = [](std::string d){};
    auto mock = std::make_shared<MockConnector>(io, write_callback);
	mock->handler(tested_object);
	auto user_handlers = tested_object->get_user_handlers();
	auto &req = user_handlers.second;
	auto &res = user_handlers.first;
	tested_object->on_request_sent([&called, &res](auto conn){
		called = true;
		//res->abort();
	});
	req->headers(std::move(preamble));
	req->end();
	res->get_connection()->close();
	mock->io_service().run();
	ASSERT_TRUE(called);
}

TEST(client_connection, read_before_write_failure)
{
    boost::asio::io_service io;
	auto tested_object = std::make_shared<client_connection_t>();
	MockConnector::wcb write_callback = [](std::string){};
	auto mock = std::make_shared<MockConnector>(io, write_callback);
	bool called;
	tested_object->on_error([&called, &mock](auto conn, const http::connection_error &ec){
		ASSERT_EQ(ec.errc(), http::error_code::invalid_read);
		called = true;
		conn->close();
		mock = nullptr;
	});

	mock->handler(tested_object);
	std::string unexpected_response = "HTTP/1.1 200 OK\r\n"
		"connection: keep-alive\r\n"
		"content-length: 33\r\n"
		"content-type: text/plain\r\n"
		"date: Tue, 17 May 2016 14:53:09 GMT\r\n"
		"\r\n"
		"Ave client, dummy node says hello";
	mock->read(unexpected_response);
	io.run();
	ASSERT_TRUE(called);
}


TEST(client_connection, pingpong)
{
    boost::asio::io_service io;


	auto tested_object = std::make_shared<client_connection_t>();
	//now register a mock connector; than go on.

	std::string body{"ciao"};
	http::http_request preamble = make_dumb_request();
	std::string received_data{};
	std::string expected_request = get_expected(preamble, body);

	MockConnector::wcb write_callback;
    auto mock = std::make_shared<MockConnector>(io, write_callback);
	bool finished_request{false};
	int  response_events{0};
	write_callback = [&, mock](std::string d)
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

	res->on_body([&](auto r, std::unique_ptr<const char[]> body, size_t s){
		std::string rcvd{body.get(), s};
		ASSERT_EQ(rcvd, "Ave client, dummy node says hello");
		++response_events;
	});

	res->on_finished([&](auto response){
		++response_events;
		response->get_connection()->close();
	});

	req->headers(std::move(preamble));
	req->body(make_data_ptr("ciao"), 4);
	req->end();
	mock->io_service().run();
	ASSERT_EQ(expected_request, received_data);
	ASSERT_TRUE(finished_request);
	ASSERT_EQ(response_events, 3);
}


TEST(client_connection, multiple_requests_multiple_responses)
{

	boost::asio::io_service io;
	boost::asio::io_service::work *w = new boost::asio::io_service::work(io);
	auto tested_object = std::make_shared<client_connection_t>();
	MockConnector::wcb write_callback;
	auto mock = std::make_shared<MockConnector>(io, write_callback);
	size_t finished_requests{0};
	size_t received_responses{0};
	mock->handler(tested_object);
	bool first_received{false}, second_received{false};
	std::string expected_first_response = "HTTP/1.1 200 OK\r\n"
			"connection: keep-alive\r\n"
			"content-length: 33\r\n"
			"content-type: text/plain\r\n"
			"date: Tue, 17 May 2016 14:53:09 GMT\r\n"
			"\r\n"
			"Ave client, dummy node says hello";
	std::string expected_second_response = "HTTP/1.1 200 OK\r\n"
			"connection: keep-alive\r\n"
			"content-length: 33\r\n"
			"content-type: text/plain\r\n"
			"date: Tue, 17 May 2016 14:53:09 GMT\r\n"
			"\r\n"
			"Ave client, dummy node says hell!";
	std::string rcvd;
	write_callback = [&](auto what){
		rcvd += std::string(what);
		if(rcvd == std::string(make_dumb_request().serialize())) {
			if(!first_received) {
				first_received = true;
				mock->read(expected_first_response);
			}
			else {
				second_received = true;
				mock->read(expected_second_response);
			}
			rcvd = "";
		}
	};



	std::string received_first_response{};
	std::string received_second_response{};
	auto user_handlers = tested_object->get_user_handlers();
	auto &req = user_handlers.second;
	auto &res = user_handlers.first;
	tested_object->on_error([](auto conn, auto& error){
		FAIL() << "there was an unexpected error, with code " << int(error.errc()) << std::endl;
	});
	req->on_write([&](auto req){
		ASSERT_EQ(finished_requests, 0U);
		++finished_requests;
		auto second_request_handlers = tested_object->get_user_handlers();
		auto second_request = second_request_handlers.second;
		auto second_response = second_request_handlers.first;

		second_request->on_write([&](auto second_request){
			ASSERT_EQ(finished_requests, 1U);
			++finished_requests;
		});

		second_response->on_headers([&received_second_response, &received_responses](auto response) mutable {
			ASSERT_EQ(received_responses, 3U);
			++received_responses;
			received_second_response = std::string(response->preamble().serialize());
		});

		second_response->on_body([&received_second_response, &received_responses](auto response, auto body, auto body_size) mutable {
			ASSERT_EQ(received_responses, 4U);
			//this might be wrong in reality, as we don't know how many body events will be emitted. However it works just fine in reality
			++received_responses;
			received_second_response += std::string{body.get(), body_size};
		});


		second_response->on_finished([&received_second_response, expected_second_response, &received_responses, &w](auto response) mutable {
			ASSERT_EQ(received_responses, 5U);
			++received_responses;
			ASSERT_EQ(received_second_response, expected_second_response);
			response->get_connection()->close();
			delete w;
		});
		auto p = make_dumb_request();
		second_request->headers(std::move(p));
		second_request->end();

	});

	res->on_headers([&](auto res){
		ASSERT_EQ(received_responses, 0U);
		++received_responses;
		received_first_response += std::string(res->preamble().serialize());
	});

	res->on_body([&](auto res, auto body, auto body_size){
		ASSERT_EQ(received_responses, 1U);
		++received_responses;
		received_first_response += std::string{body.get(), body_size};
	});

	res->on_finished([&](auto res){
		ASSERT_EQ(received_responses, 2U);
		++received_responses;
		ASSERT_EQ(received_first_response, expected_first_response);

	});

	req->headers(make_dumb_request());
	req->end();

	io.run();
}


TEST(client_connection, response_continue)
{
	boost::asio::io_service io;


	auto tested_object = std::make_shared<client_connection_t>();
	//now register a mock connector; than go on.

	std::string body{"ciao"};
	http::http_request preamble = make_dumb_request();
	std::string received_data{};
	std::string expected_request = get_expected(preamble, body);

	MockConnector::wcb write_callback;
	auto mock = std::make_shared<MockConnector>(io, write_callback);
	write_callback = [](auto &&d) {};
	mock->handler(tested_object);
	auto user_handlers = tested_object->get_user_handlers();
	auto req = user_handlers.second;
	auto res = user_handlers.first;
	req->headers(make_dumb_request());
	io.post([mock, req](){
		mock->read("HTTP/1.1 100 Continue\r\n\r\n");
		req->end();
	});
	bool called_continue{false};
	res->on_response_continue([&called_continue, &tested_object](auto res){
		called_continue = true;
		res->get_connection()->close();
	});

	io.run();

	ASSERT_TRUE(called_continue);
}

TEST(client_connection, pipeline)
{
	boost::asio::io_service io;
	auto tested_object = std::make_shared<client_connection_t>();
	auto r1 = make_dumb_request();
	auto r2 = make_dumb_request();
	r1.header("request-id", "1");
	r2.header("request-id", "2");

	auto first_req_serialization = std::string(r1.serialize());
	auto second_req_serialization = std::string(r2.serialize());

	std::string rcvd{};
	MockConnector::wcb write_callback;
	auto mock = std::make_shared<MockConnector>(io, write_callback);
	mock->handler(tested_object);
	int rcvd_rqs = 0;
	write_callback = [&](std::string d)
	{
		if(d.empty()) return;
		rcvd += std::string(d);
		if (rcvd == first_req_serialization)
		{
			ASSERT_EQ(rcvd_rqs, 0);
			++rcvd_rqs;
			rcvd = {};
		}
		if(rcvd == second_req_serialization)
		{
			ASSERT_EQ(rcvd_rqs, 1);
			++rcvd_rqs;
			rcvd = {};
		}

		if(rcvd_rqs == 2)
		{
			std::string first_response = "HTTP/1.1 200 OK\r\n"
					"connection: keep-alive\r\n"
					"content-length: 33\r\n"
					"content-type: text/plain\r\n"
					"request-id: 1\r\n"
					"date: Tue, 17 May 2016 14:53:09 GMT\r\n"
					"\r\n"
					"Ave client, dummy node says hell!";
			std::string second_response = "HTTP/1.1 200 OK\r\n"
					"connection: keep-alive\r\n"
					"content-length: 33\r\n"
					"content-type: text/plain\r\n"
					"request-id: 2\r\n"
					"date: Tue, 17 May 2016 14:53:09 GMT\r\n"
					"\r\n"
					"Ave client, dummy node says hell!";
			mock->read(first_response);
			mock->read(second_response);
		}
	};

	auto r1_handlers = tested_object->get_user_handlers();
	auto r2_handlers = tested_object->get_user_handlers();
	auto &req1 = r1_handlers.second;
	auto &res1 = r1_handlers.first;
	auto &req2 = r2_handlers.second;
	auto &res2 = r2_handlers.first;

	int response_cb_count{0};
	res1->on_headers([&response_cb_count](auto res1){
		ASSERT_EQ(response_cb_count, 0);
		ASSERT_TRUE(res1->preamble().has("request-id"));
		ASSERT_EQ(std::string(res1->preamble().header("request-id")), "1");
		++response_cb_count;
	});

	res2->on_headers([&response_cb_count](auto res2){
		ASSERT_EQ(response_cb_count, 1);
		ASSERT_TRUE(res2->preamble().has("request-id"));
		ASSERT_EQ(std::string(res2->preamble().header("request-id")), "2");
		++response_cb_count;
	});

	req1->headers(std::move(r1));
	req1->end();
	req2->headers(std::move(r2));
	req2->end();

	io.run();

	ASSERT_EQ(response_cb_count, 2);
	ASSERT_EQ(rcvd_rqs, 2);
}


TEST(client_connection, http11_persistent)
{
	boost::asio::io_service io;
	boost::asio::io_service::work *w = new boost::asio::io_service::work(io);
	auto tested_object = std::make_shared<client_connection_t>();
	MockConnector::wcb write_callback;
	auto mock = std::make_shared<MockConnector>(io, write_callback);
	int req_chunks_rcvd{0};
	write_callback = [&](auto d)
	{
		++req_chunks_rcvd;
		if(req_chunks_rcvd == 2)
		{
			mock->read("HTTP/1.1 200 OK\r\n"
					           "content-length: 0\r\n\r\n");
		}
	};
	mock->handler(tested_object);
	auto r1 = make_dumb_request();
	auto handlers = tested_object->get_user_handlers();
	auto &req = handlers.second;
	auto &res = handlers.first;

	io.post([r1, req]() mutable {
		req->headers(std::move(r1));
		req->end();
	});

	int res_rcvd{0};
	res->on_headers([&res_rcvd](auto r) {
		ASSERT_EQ(res_rcvd, 0);
		++res_rcvd;
	});

	res->on_finished([w, &res_rcvd](auto r){
		ASSERT_EQ(res_rcvd, 1);
		++res_rcvd;
		delete w;
	});

	io.run();
	ASSERT_FALSE(tested_object->should_stop());
}


TEST(client_connection, http11_not_persistent_request)
{
	boost::asio::io_service io;
	boost::asio::io_service::work *w = new boost::asio::io_service::work(io);
	auto tested_object = std::make_shared<client_connection_t>();
	auto r1 = make_dumb_request();
	r1.header("connection", "close");
	MockConnector::wcb write_callback;
	auto mock = std::make_shared<MockConnector>(io, write_callback);
	int req_chunks_rcvd{0};
	write_callback = [&](auto d)
	{
		++req_chunks_rcvd;
		if(req_chunks_rcvd == 2)
		{
			mock->read("HTTP/1.1 200 OK\r\n"
					           "content-length: 0\r\n\r\n");
		}
	};
	mock->handler(tested_object);
	auto handlers = tested_object->get_user_handlers();
	auto &req = handlers.second;
	auto &res = handlers.first;
	io.post([req, r1]() mutable {
		req->headers(std::move(r1));
		req->end();
	});

	int res_rcvd{0};
	res->on_headers([&res_rcvd](auto r){
		ASSERT_EQ(res_rcvd, 0);
		++res_rcvd;
	});

	res->on_finished([w, &res_rcvd](auto r){
		ASSERT_EQ(res_rcvd, 1);
		++res_rcvd;
		delete w;
	});

	io.run();
	ASSERT_TRUE(tested_object->should_stop());
}



TEST(client_connection, http11_not_persistent_response)
{
	boost::asio::io_service io;
	boost::asio::io_service::work *w = new boost::asio::io_service::work(io);
	auto tested_object = std::make_shared<client_connection_t>();
	auto r1 = make_dumb_request();
	MockConnector::wcb write_callback;
	auto mock = std::make_shared<MockConnector>(io, write_callback);
	int req_chunks_rcvd{0};
	write_callback = [&](auto d)
	{
		++req_chunks_rcvd;
		if(req_chunks_rcvd == 2)
		{
			mock->read("HTTP/1.1 200 OK\r\n"
					   "connection: close\r\n"
					   "content-length: 0\r\n\r\n");
		}
	};
	mock->handler(tested_object);
	auto handlers = tested_object->get_user_handlers();
	auto &req = handlers.second;
	auto &res = handlers.first;
	io.post([req, r1]() mutable {
		req->headers(std::move(r1));
		req->end();
	});

	int res_rcvd{0};
	res->on_headers([&res_rcvd](auto r){
		ASSERT_EQ(res_rcvd, 0);
		++res_rcvd;
	});

	res->on_finished([w, &res_rcvd](auto r){
		ASSERT_EQ(res_rcvd, 1);
		++res_rcvd;
		delete w;
	});

	io.run();
	ASSERT_TRUE(tested_object->should_stop());
}


TEST(client_connection, http10_not_persistent_request)
{
	boost::asio::io_service io;
	boost::asio::io_service::work *w = new boost::asio::io_service::work(io);
	auto tested_object = std::make_shared<client_connection_t>();
	auto r1 = make_dumb_request();
	r1.protocol(http::proto_version::HTTP10);

	MockConnector::wcb write_callback;
	auto mock = std::make_shared<MockConnector>(io, write_callback);
	int req_chunks_rcvd{0};
	write_callback = [&](auto d)
	{
		++req_chunks_rcvd;
		if(req_chunks_rcvd == 2)
		{
			mock->read("HTTP/1.0 200 OK\r\n"
					           "content-length: 0\r\n\r\n");
		}
	};
	mock->handler(tested_object);
	auto handlers = tested_object->get_user_handlers();
	auto &req = handlers.second;
	auto &res = handlers.first;
	io.post([req, r1]() mutable
	{
		req->headers(std::move(r1));
		req->end();
	});

	int res_rcvd{0};
	res->on_headers([&res_rcvd](auto r){
		ASSERT_EQ(res_rcvd, 0);
		++res_rcvd;
	});

	res->on_finished([w, &res_rcvd](auto r){
		ASSERT_EQ(res_rcvd, 1);
		++res_rcvd;
		delete w;
	});

	io.run();
	ASSERT_TRUE(tested_object->should_stop());
}


TEST(client_connection, http10_not_persistent_response)
{
	boost::asio::io_service io;
	boost::asio::io_service::work *w = new boost::asio::io_service::work(io);
	auto tested_object = std::make_shared<client_connection_t>();
	auto r1 = make_dumb_request();
	r1.protocol(http::proto_version::HTTP10);
	r1.header("connection", "keep-alive");
	MockConnector::wcb write_callback;
	auto mock = std::make_shared<MockConnector>(io, write_callback);
	int req_chunks_rcvd{0};
	write_callback = [&](auto d)
	{
		++req_chunks_rcvd;
		if(req_chunks_rcvd == 2)
		{
			mock->read("HTTP/1.0 200 OK\r\n"
					           "content-length: 0\r\n\r\n");
		}
	};
	mock->handler(tested_object);
	auto handlers = tested_object->get_user_handlers();
	auto &req = handlers.second;
	auto &res = handlers.first;
	io.post([req, r1]() mutable {
		req->headers(std::move(r1));
		req->end();
	});

	int res_rcvd{0};
	res->on_headers([&res_rcvd](auto r){
		ASSERT_EQ(res_rcvd, 0);
		++res_rcvd;
	});

	res->on_finished([w, &res_rcvd](auto r){
		ASSERT_EQ(res_rcvd, 1);
		++res_rcvd;
		delete w;
	});

	io.run();
	ASSERT_TRUE(tested_object->should_stop());
}


TEST(client_connection, http10_persistent)
{
	boost::asio::io_service io;
	boost::asio::io_service::work *w = new boost::asio::io_service::work(io);
	auto tested_object = std::make_shared<client_connection_t>();
	auto r1 = make_dumb_request();
	r1.protocol(http::proto_version::HTTP10);
	r1.header("connection", "keep-alive");

	MockConnector::wcb write_callback;
	auto mock = std::make_shared<MockConnector>(io, write_callback);
	int req_chunks_rcvd{0};
	write_callback = [&](auto d)
	{
		++req_chunks_rcvd;
		if(req_chunks_rcvd == 2)
		{
			mock->read("HTTP/1.0 200 OK\r\n"
						"connection: keep-alive\r\n"
					           "content-length: 0\r\n\r\n");
		}
	};
	mock->handler(tested_object);
	auto handlers = tested_object->get_user_handlers();
	auto &req = handlers.second;
	auto &res = handlers.first;
	io.post([req, r1]() mutable {
		req->headers(std::move(r1));
		req->end();
	});


	int res_rcvd{0};
	res->on_headers([&res_rcvd](auto r){
		ASSERT_EQ(res_rcvd, 0);
		++res_rcvd;
	});

	res->on_finished([w, &res_rcvd](auto r){
		ASSERT_EQ(res_rcvd, 1);
		++res_rcvd;
		delete w;
	});

	io.run();
	ASSERT_FALSE(tested_object->should_stop());
}


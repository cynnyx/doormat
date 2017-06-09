#include "../src/http/server/server_connection.h"
#include "../src/protocol/handler_http1.h"
#include "../src/http/server/server_traits.h"
#include "mocks/mock_connector/mock_connector.h"
#include <gtest/gtest.h>
#include <gmock/gmock.h>


using server_connection_t = server::handler_http1<http::server_traits>;

// todo: why do we need this copy?
static std::unique_ptr<char[]> make_data_ptr(const std::string& s)
{
	auto ptr = std::make_unique<char[]>(s.size());
	std::copy(s.begin(), s.end(), ptr.get());
	return ptr;
}

class server_connection_test : public ::testing::Test
{
public:
	void SetUp()
	{
		_write_cb = [](auto) {};
		mock_connector = std::make_shared<MockConnector>(io, _write_cb);
		_handler = std::make_shared<server_connection_t>();
		mock_connector->handler(_handler);
		response = "";
	}

	boost::asio::io_service io;
	std::shared_ptr<MockConnector> mock_connector;
	std::shared_ptr<server_connection_t> _handler;
	MockConnector::wcb _write_cb;
	std::string response;
};


TEST_F(server_connection_test, invalid_request)
{
	bool called{false};

	_handler->on_request([&called](auto conn, auto req, auto resp) {
		req->on_error([&called](auto conn, auto &error) {
			ASSERT_TRUE(conn);
			ASSERT_EQ(error.errc(), http::error_code::decoding);
			called = true;
		});
	});

	mock_connector->io_service().post([this]() {
		mock_connector->read("GET /prova HTTP_WRONG/1.1");
	});

	mock_connector->io_service().run();

	ASSERT_TRUE(called);
}


TEST_F(server_connection_test, valid_request)
{
	size_t expected_cb_counter{2};
	size_t cb_counter{0};
	_handler->on_request([&cb_counter](auto conn, auto req, auto resp) {
		req->on_headers([&cb_counter](auto req) {
			++cb_counter;
		});

		req->on_finished([&cb_counter](auto req) {
			++cb_counter;
		});
	});

	mock_connector->io_service().post([this]() {
		mock_connector->read("GET / HTTP/1.1\r\n"
									 "host:localhost:1443\r\n"
									 "date: Tue, 17 May 2016 14:53:09 GMT\r\n"
									 "\r\n");
	});
	mock_connector->io_service().run();
	ASSERT_EQ(cb_counter, expected_cb_counter);
}


TEST_F(server_connection_test, on_connector_nulled)
{
	size_t expected_req_cb_counter{2};
	size_t expected_error_cb_counter{1};

	size_t req_cb_counter{0};
	size_t error_cb_counter{0};

	_handler->on_request([&req_cb_counter, this, &error_cb_counter](auto conn, auto req, auto resp) {
		req->on_headers([&req_cb_counter](auto req) {
			++req_cb_counter;
		});

		req->on_finished([&req_cb_counter, this, resp](auto req) {
			++req_cb_counter;
			mock_connector = nullptr;
			resp->headers(http::http_response{});
		});

		conn->on_error([&error_cb_counter](auto conn, const http::connection_error &error) {
			ASSERT_TRUE(conn);
			ASSERT_EQ(error.errc(), http::error_code::closed_by_client);
			++error_cb_counter;
		});
	});

	mock_connector->io_service().post([this]() {
		mock_connector->read("GET / HTTP/1.1\r\n"
				                     "host:localhost:1443\r\n"
				                     "date: Tue, 17 May 2016 14:53:09 GMT\r\n"
				                     "\r\n");
	});

	mock_connector->io_service().run();
	ASSERT_EQ(req_cb_counter, expected_req_cb_counter);
	ASSERT_EQ(error_cb_counter, expected_error_cb_counter);
}

TEST_F(server_connection_test, on_connection_closed)
{
	size_t expected_req_cb_counter{2};
	size_t req_cb_counter{0};

	bool err_cb_called{false};

	std::shared_ptr<http::response> res = nullptr;

	_handler->on_request([&req_cb_counter, this, &err_cb_called, &res](auto conn, auto req, auto resp) {
		res = resp;

		req->on_headers([&req_cb_counter](auto req) {
			++req_cb_counter;
		});

		req->on_finished([&req_cb_counter, this](auto req) {
			++req_cb_counter;
			req->get_connection()->close();
		});

		resp->on_error([&err_cb_called](){
			err_cb_called = true;
		});
	});

	mock_connector->io_service().post([this]() {
		mock_connector->read("GET / HTTP/1.1\r\n"
				                     "host:localhost:1443\r\n"
				                     "date: Tue, 17 May 2016 14:53:09 GMT\r\n"
				                     "\r\n");
	});

	mock_connector->io_service().run();
	ASSERT_EQ(req_cb_counter, expected_req_cb_counter);
	ASSERT_TRUE(err_cb_called);
}

TEST_F(server_connection_test, response_continue)
{
	std::string accumulator{};
	bool rcvd{false};
	_write_cb = [&accumulator, &rcvd](std::string chunk) {
		accumulator += std::string(chunk);
		if (accumulator == "HTTP/1.1 100 Continue\r\n"
				                   "content-length: 0\r\n"
				                   "\r\n") {
			rcvd = true;
		}
	};

	_handler->on_request([](auto conn, auto req, auto resp) {
		req->on_headers([resp](auto req) {
			resp->send_continue();
		});

	});

	mock_connector->io_service().post([this]() {
		mock_connector->read("GET / HTTP/1.1\r\n"
									 "host:localhost:1443\r\n"
									 "date: Tue, 17 May 2016 14:53:09 GMT\r\n"
									 "\r\n");
	});
	mock_connector->io_service().run();
	ASSERT_TRUE(rcvd);
}


TEST_F(server_connection_test, http11_persistent)
{
	_handler->set_persistent(true);
	_handler->on_request([&](auto conn, auto req, auto res) {
		req->on_finished([res](auto req) {
			http::http_response r;
			r.protocol(http::proto_version::HTTP11);
			std::string body{"Ave client, dummy node says hello"};
			r.status(200);
			r.keepalive(true);
			r.header("content-type", "text/plain");
			r.header("date", "Tue, 17 May 2016 14:53:09 GMT");
			r.content_len(body.size());
			res->headers(std::move(r));
			res->body(make_data_ptr(body), body.size());
			res->end();
		});
	});

	// list of chunks that I expect to receive inside the on_write()
	std::string expected_response = "HTTP/1.1 200 OK\r\n"
			"connection: keep-alive\r\n"
			"content-length: 33\r\n"
			"content-type: text/plain\r\n"
			"date: Tue, 17 May 2016 14:53:09 GMT\r\n"
			"\r\n"
			"Ave client, dummy node says hello";

	bool terminated{false};
	_write_cb = [this, &expected_response, &terminated](std::string chunk) {
		response.append(chunk);
		if (response == expected_response) {
			terminated = true;
		}
	};

	mock_connector->io_service().post([this]() {
		mock_connector->read("GET / HTTP/1.1\r\n"
									 "host:localhost:1443\r\n"
									 "date: Tue, 17 May 2016 14:53:09 GMT\r\n"
									 "\r\n");
	});
	mock_connector->io_service().run();
	ASSERT_FALSE(_handler->should_stop());
	ASSERT_TRUE(terminated);
	ASSERT_TRUE(response.find("connection: keep-alive\r\n") != std::string::npos);
}


TEST_F(server_connection_test, http11_non_persistent)
{
	_handler->set_persistent(false);
	_handler->on_request([&](auto conn, auto req, auto res) {
		req->on_finished([res](auto req) {
			http::http_response r;
			r.protocol(http::proto_version::HTTP11);
			std::string body{"Ave client, dummy node says hello"};
			r.status(200);
			r.keepalive(false);
			r.header("content-type", "text/plain");
			r.header("date", "Tue, 17 May 2016 14:53:09 GMT");
			r.content_len(body.size());
			res->headers(std::move(r));
			res->body(make_data_ptr(body), body.size());
			res->end();
		});
	});

	// list of chunks that I expect to receive inside the on_write()
	std::string expected_response = "HTTP/1.1 200 OK\r\n"
			"connection: close\r\n"
			"content-length: 33\r\n"
			"content-type: text/plain\r\n"
			"date: Tue, 17 May 2016 14:53:09 GMT\r\n"
			"\r\n"
			"Ave client, dummy node says hello";

	bool terminated{false};
	_write_cb = [this, &terminated, &expected_response](std::string chunk) {
		response.append(chunk);
		if (response == expected_response) {
			terminated = true;
		}
	};

	mock_connector->io_service().post([this]() {
		mock_connector->read("GET / HTTP/1.1\r\n"
			"host:localhost:1443\r\n"
			"date: Tue, 17 May 2016 14:53:09 GMT\r\n"
			"connection: close\r\n"
			"\r\n");
	});

	mock_connector->io_service().run();
	ASSERT_TRUE(_handler->should_stop());
	ASSERT_TRUE(terminated);
	ASSERT_TRUE(response.find("connection: close\r\n") != std::string::npos);
}


TEST_F(server_connection_test, http11_pipelining)
{
	_handler->set_persistent(true);
	_handler->on_request([&](auto conn, auto req, auto res) {
		req->on_finished([res](auto req) {
			bool keep_alive = req->preamble().keepalive();
			http::http_response r;
			r.protocol(http::proto_version::HTTP11);
			std::string body{"Ave client, dummy node says hello"};
			r.status(200);
			r.keepalive(keep_alive);
			r.header("content-type", "text/plain");
			r.header("date", "Tue, 17 May 2016 14:53:09 GMT");
			r.content_len(body.size());
			res->headers(std::move(r));
			res->body(make_data_ptr(body), body.size());
			res->end();
		});
	});

	// list of chunks that I expect to receive inside the on_write()
	std::string expected_response =
			"HTTP/1.1 200 OK\r\n"
					"connection: keep-alive\r\n"
					"content-length: 33\r\n"
					"content-type: text/plain\r\n"
					"date: Tue, 17 May 2016 14:53:09 GMT\r\n"
					"\r\n"
					"Ave client, dummy node says hello"
					"HTTP/1.1 200 OK\r\n"
					"connection: close\r\n"
					"content-length: 33\r\n"
					"content-type: text/plain\r\n"
					"date: Tue, 17 May 2016 14:53:09 GMT\r\n"
					"\r\n"
					"Ave client, dummy node says hello";

	bool terminated{false};
	_write_cb = [this, &terminated, &expected_response](std::string chunk) {
		response.append(chunk);
		if (response == expected_response) {
			terminated = true;
		}
	};

	mock_connector->io_service().post([this]() 
	{
		mock_connector->read("GET / HTTP/1.1\r\n"
				"host:localhost:1443\r\n"
				"date: Tue, 17 May 2016 14:53:09 GMT\r\n"
				"\r\n"
				"GET / HTTP/1.1\r\n"
				"host:localhost:1443\r\n"
				"date: Tue, 17 May 2016 14:53:09 GMT\r\n"
				"connection: close\r\n"
				"\r\n");
	});

	mock_connector->io_service().run();
	ASSERT_TRUE(_handler->should_stop());
	ASSERT_TRUE(terminated);
	ASSERT_TRUE(response.find("connection: close\r\n") != std::string::npos);
}


TEST_F(server_connection_test, http11_missing_response)
{
	_handler->set_persistent(true);
	int req_counter{0};

	_handler->on_request([&](auto conn, auto req, auto res) {
		++req_counter;
		if(req_counter == 2)
			return;

		req->on_finished([res](auto req) {
			bool keep_alive = req->preamble().keepalive();
			http::http_response r;
			r.protocol(http::proto_version::HTTP11);
			std::string body{"Ave client, dummy node says hello"};
			r.status(200);
			r.keepalive(keep_alive);
			r.header("content-type", "text/plain");
			r.header("date", "Tue, 17 May 2016 14:53:09 GMT");
			r.content_len(body.size());
			res->headers(std::move(r));
			res->body(make_data_ptr(body), body.size());
			res->end();
		});

		conn->on_error([](auto conn, const http::connection_error &error) {
			ASSERT_TRUE(conn);
			ASSERT_EQ(error.errc(), http::error_code::missing_stream_element);
			conn->close();
		});
	});

	// list of chunks that I expect to receive inside the on_write()
	std::string expected_response =
			"HTTP/1.1 200 OK\r\n"
			"connection: keep-alive\r\n"
			"content-length: 33\r\n"
			"content-type: text/plain\r\n"
			"date: Tue, 17 May 2016 14:53:09 GMT\r\n"
			"\r\n"
			"Ave client, dummy node says hello";

	bool terminated{false};
	_write_cb = [this, &terminated, &expected_response](std::string chunk)
	{
		response.append(chunk);
		if (response == expected_response)
			terminated = true;
	};

	mock_connector->io_service().post([this]() 
	{
		mock_connector->read("GET / HTTP/1.1\r\n"
				"host:localhost:1443\r\n"
				"date: Tue, 17 May 2016 14:53:09 GMT\r\n"
				"\r\n"
				"GET / HTTP/1.1\r\n"
				"host:localhost:1443\r\n"
				"date: Tue, 17 May 2016 14:53:09 GMT\r\n"
				"\r\n"
				"GET / HTTP/1.1\r\n"
				"host:localhost:1443\r\n"
				"date: Tue, 17 May 2016 14:53:09 GMT\r\n"
				"connection: close\r\n"
				"\r\n");
	});

	mock_connector->io_service().run();
	ASSERT_TRUE(terminated);
	ASSERT_TRUE(response.find("connection: keep-alive\r\n") != std::string::npos);
}


TEST_F(server_connection_test, http10_persistent)
{
	_handler->set_persistent(false);
	_handler->on_request([&](auto conn, auto req, auto res) {
		req->on_finished([res](auto req) {
			http::http_response r;
			r.protocol(http::proto_version::HTTP10);
			std::string body{"Ave client, dummy node says hello"};
			r.status(200);
			r.keepalive(false);
			r.header("content-type", "text/plain");
			r.header("date", "Tue, 17 May 2016 14:53:09 GMT");
			r.content_len(body.size());
			res->headers(std::move(r));
			res->body(make_data_ptr(body), body.size());
			res->end();
		});
	});

	// list of chunks that I expect to receive inside the on_write()
	std::string expected_response = "HTTP/1.0 200 OK\r\n"
			"connection: close\r\n"
			"content-length: 33\r\n"
			"content-type: text/plain\r\n"
			"date: Tue, 17 May 2016 14:53:09 GMT\r\n"
			"\r\n"
			"Ave client, dummy node says hello";

	bool terminated{false};
	_write_cb = [this, &terminated, &expected_response](std::string chunk) {
		response.append(chunk);
		if (response == expected_response) {
			terminated = true;
		}
	};

	mock_connector->io_service().post([this]() {
		mock_connector->read("GET / HTTP/1.0\r\n"
				                     "date: Tue, 17 May 2016 14:53:09 GMT\r\n"
				                     "connection: keep-alive\r\n"
				                     "\r\n");
	});

	mock_connector->io_service().run();
	ASSERT_TRUE(_handler->should_stop());
	ASSERT_TRUE(terminated);
	ASSERT_TRUE(response.find("connection: close\r\n") != std::string::npos);
}


TEST_F(server_connection_test, http10_non_persistent)
{
	_handler->set_persistent(false);
	_handler->on_request([&](auto conn, auto req, auto res) {
		req->on_finished([res](auto req) {
			http::http_response r;
			r.protocol(http::proto_version::HTTP10);
			std::string body{"Ave client, dummy node says hello"};
			r.status(200);
			r.keepalive(false);
			r.header("content-type", "text/plain");
			r.header("date", "Tue, 17 May 2016 14:53:09 GMT");
			r.content_len(body.size());
			res->headers(std::move(r));
			res->body(make_data_ptr(body), body.size());
			res->end();
		});
	});

	// list of chunks that I expect to receive inside the on_write()
	std::string expected_response = "HTTP/1.0 200 OK\r\n"
			"connection: close\r\n"
			"content-length: 33\r\n"
			"content-type: text/plain\r\n"
			"date: Tue, 17 May 2016 14:53:09 GMT\r\n"
			"\r\n"
			"Ave client, dummy node says hello";

	bool terminated{false};
	_write_cb = [this, &terminated, &expected_response](std::string chunk) {
		response.append(chunk);
		if (response == expected_response) {
			terminated = true;
		}
	};

	mock_connector->io_service().post([this]() {
		mock_connector->read("GET / HTTP/1.0\r\n"
				                     "date: Tue, 17 May 2016 14:53:09 GMT\r\n"
				                     "connection: close\r\n"
				                     "\r\n");
	});

	mock_connector->io_service().run();
	ASSERT_TRUE(_handler->should_stop());
	ASSERT_TRUE(terminated);
	ASSERT_TRUE(response.find("connection: close\r\n") != std::string::npos);
}


TEST_F(server_connection_test, http10_pipelining)
{
	_handler->set_persistent(true);
	_handler->on_request([&](auto conn, auto req, auto res) 
	{
		req->on_finished([res](auto req) {
			bool keep_alive = req->preamble().keepalive();
			http::http_response r;
			r.protocol(http::proto_version::HTTP10);
			std::string body{"Ave client, dummy node says hello"};
			r.status(200);
			r.keepalive(keep_alive);
			r.header("content-type", "text/plain");
			r.header("date", "Tue, 17 May 2016 14:53:09 GMT");
			r.content_len(body.size());
			res->headers(std::move(r));
			res->body(make_data_ptr(body), body.size());
			res->end();
		});
	});

	// list of chunks that I expect to receive inside the on_write()
	std::string expected_response = "HTTP/1.0 200 OK\r\n"
					"connection: keep-alive\r\n"
					"content-length: 33\r\n"
					"content-type: text/plain\r\n"
					"date: Tue, 17 May 2016 14:53:09 GMT\r\n"
					"\r\n"
					"Ave client, dummy node says hello"
					"HTTP/1.0 200 OK\r\n"
					"connection: close\r\n"
					"content-length: 33\r\n"
					"content-type: text/plain\r\n"
					"date: Tue, 17 May 2016 14:53:09 GMT\r\n"
					"\r\n"
					"Ave client, dummy node says hello";

	bool terminated{false};
	_write_cb = [this, &terminated, &expected_response](auto chunk) 
	{
		response.append(chunk);
		if (response == expected_response) 
		{
			terminated = true;
		}
	};

	mock_connector->io_service().post([this]() 
	{
		mock_connector->read("GET / HTTP/1.0\r\n"
			"date: Tue, 17 May 2016 14:53:09 GMT\r\n"
			"connection: keep-alive\r\n"
			"\r\n"
			"GET / HTTP/1.0\r\n"
			"date: Tue, 17 May 2016 14:53:09 GMT\r\n"
			"connection: close\r\n"
			"\r\n");
	});

	mock_connector->io_service().run();
	ASSERT_TRUE(_handler->should_stop());
	ASSERT_TRUE(terminated);
	ASSERT_TRUE(response.find("connection: close\r\n") != std::string::npos);
}

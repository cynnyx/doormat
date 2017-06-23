#include <gtest/gtest.h>

#include "src/http2/session_client.h"
#include "src/http2/session.h"
#include "src/http/server/request.h"
#include "src/http/server/response.h"
#include "src/http/client/request.h"
#include "src/http/client/response.h"
#include "mocks/mock_connector/mock_connector.h"

#include "boost/asio.hpp"

#include "../../../src/utils/log_wrapper.h"

static std::unique_ptr<char[]> make_data_ptr(const std::string& s)
{
	auto ptr = std::make_unique<char[]>(s.size());
	std::copy(s.begin(), s.end(), ptr.get());
	return std::move(ptr);
}

TEST(http2_session_client_test, first_request)
{
	static bool headers{false};
	static bool finished{false};
	std::function<void(std::string)> wcb_c;
	std::function<void(std::string)> wcb_s;
	boost::asio::io_service io;
	auto keep_alive = std::make_unique<boost::asio::io_service::work>(io);
	auto conn_c = std::make_shared<MockConnector>(io, wcb_c);
	auto conn_s = std::make_shared<MockConnector>(io, wcb_s);
	wcb_c = [&conn_s, &io](auto&& s) 
	{
		io.post([&conn_s, s=std::move(s)] { conn_s->read(std::move(s)); });
	};
	wcb_s = [&conn_c, &io](auto&& s) 
	{
		io.post([&conn_c, s=std::move(s)] { conn_c->read(std::move(s)); });
	};

	auto server = std::make_shared<http2::session>();
	server->connector(conn_s);
	conn_s->handler(server);
	server->on_request([](auto conn, auto req, auto res) 
	{
		req->on_headers([](auto)
		{
			//std::cout <<" youbeeeeeeeeee\n" << std::endl;
			headers = true;
		});
		req->on_finished([res=std::move(res)](auto&&) {
			static const std::string body{"Allright"};
			http::http_response r;
			r.protocol(http::proto_version::HTTP20);
			r.status(200);
			r.header("content-type", "text/plain");
			r.header("date", "Tue, 17 May 2016 14:53:09 GMT");
			r.hostname("doormat_app.org");
			r.content_len(body.size());
			res->headers(std::move(r));
			res->body(make_data_ptr(body), body.size());
			res->end();
		});
	});

	auto client = std::make_shared<http2::session_client>();
	client->connector(conn_c);
	conn_c->handler(client);

	std::shared_ptr<http::client_request> req;
	std::shared_ptr<http::client_response> res;
	std::tie(req, res) = client->create_transaction();

	auto host = "gstatic.com";
	http::http_request preamble{};
	preamble.schema("schema");
	preamble.hostname(host);
	req->headers(std::move(preamble));
	req->end();
	res->on_finished([&keep_alive, &server, &client](auto res) 
	{
		finished = true;
		keep_alive.reset();
		server->close();
		client->close();
		LOGTRACE("Finished: ", res->preamble().serialize()  );
	});

	io.run();
	
	EXPECT_TRUE( finished );
	EXPECT_TRUE( headers );
}

TEST(http2_session_client_test, two_requests)
{
	static int headers{0};
	static int finished{0};
	std::function<void(std::string)> wcb_c;
	std::function<void(std::string)> wcb_s;
	boost::asio::io_service io;
	auto keep_alive = std::make_unique<boost::asio::io_service::work>(io);
	auto conn_c = std::make_shared<MockConnector>(io, wcb_c);
	auto conn_s = std::make_shared<MockConnector>(io, wcb_s);
	wcb_c = [&conn_s, &io](auto&& s) 
	{
		io.post([&conn_s, s=std::move(s)] { conn_s->read(std::move(s)); });
	};
	wcb_s = [&conn_c, &io](auto&& s) 
	{
		io.post([&conn_c, s=std::move(s)] { conn_c->read(std::move(s)); });
	};

	auto server = std::make_shared<http2::session>();
	server->connector(conn_s);
	conn_s->handler(server);
	server->on_request([](auto conn, auto req, auto res) 
	{
		req->on_headers([](auto)
		{
			++headers;
		});
		req->on_finished([res=std::move(res)](auto&&) {
			static const std::string body{"Allright"};
			http::http_response r;
			r.protocol(http::proto_version::HTTP20);
			r.status(200);
			r.header("content-type", "text/plain");
			r.header("date", "Tue, 17 May 2016 14:53:09 GMT");
			r.hostname("doormat_app.org");
			r.content_len(body.size());
			res->headers(std::move(r));
			res->body(make_data_ptr(body), body.size());
			res->end();
		});
	});

	auto client = std::make_shared<http2::session_client>();
	client->connector(conn_c);
	conn_c->handler(client);

	std::shared_ptr<http::client_request> req;
	std::shared_ptr<http::client_response> res;
	std::tie(req, res) = client->create_transaction();
	
	std::shared_ptr<http::client_request> req2;
	std::shared_ptr<http::client_response> res2;
	std::tie(req2, res2) = client->create_transaction();

	auto host = "gstatic.com";
	http::http_request preamble{};
	preamble.schema("schema");
	preamble.hostname(host);
	req->headers(std::move(preamble));
	req->end();
	res->on_finished([&keep_alive, &server, &client](auto res) 
	{
		++finished;
		if ( finished == 2 )
		{
			keep_alive.reset();
			server->close();
			client->close();
		}
		LOGTRACE("Finished: ", res->preamble().serialize()  );
	});
	
	http::http_request preamble2{};
	preamble2.schema("schema");
	preamble2.hostname(host);
	req2->headers(std::move(preamble2));
	req2->end();
	res2->on_finished([&keep_alive, &server, &client](auto res) 
	{
		++finished;
		if ( finished == 2 )
		{
			keep_alive.reset();
			server->close();
			client->close();
		}
		LOGTRACE("Finished: ", res->preamble().serialize()  );
	});
	
	io.run();
	
	EXPECT_EQ( finished, 2 );
	EXPECT_EQ( headers, 2 );
}

static inline std::size_t overflowing_frame_size( std::size_t size_ )
{
	return 1 + 16384 / size_;
}

TEST(http2_session_client_test, chunked)
{
	static int headers{0};
	static int finished{0};
	static std::size_t real_size{0};
	static std::string real_body{};

	std::function<void(std::string)> wcb_c;
	std::function<void(std::string)> wcb_s;
	boost::asio::io_service io;
	auto keep_alive = std::make_unique<boost::asio::io_service::work>(io);
	auto conn_c = std::make_shared<MockConnector>(io, wcb_c);
	auto conn_s = std::make_shared<MockConnector>(io, wcb_s);
	wcb_c = [&conn_s, &io](auto&& s) 
	{
		io.post([&conn_s, s=std::move(s)] { conn_s->read(std::move(s)); });
	};
	wcb_s = [&conn_c, &io](auto&& s) 
	{
		io.post([&conn_c, s=std::move(s)] { conn_c->read(std::move(s)); });
	};

	auto server = std::make_shared<http2::session>();
	server->connector(conn_s);
	conn_s->handler(server);
	server->on_request([](auto conn, auto req, auto res) 
	{
		req->on_headers([](auto)
		{
			++headers;
		});
		req->on_finished([res=std::move(res)](auto&&) 
		{
			static const std::string body{"Allright"};
			real_size = body.size();
			std::size_t multiplier = overflowing_frame_size( real_size );
			real_size *= multiplier;

			http::http_response r;
			r.protocol(http::proto_version::HTTP20);
			r.status(200);
			r.header("content-type", "text/plain");
			r.header("date", "Tue, 17 May 2016 14:53:09 GMT");
			r.hostname("doormat_app.org");
			r.content_len(real_size);
			res->headers(std::move(r));
			for ( std::size_t i = 0; i < multiplier; ++i )
			{
				res->body( make_data_ptr( body ), body.size());
				real_body += body;
			}
			res->end();
		});
	});

	auto client = std::make_shared<http2::session_client>();
	client->connector(conn_c);
	conn_c->handler(client);

	std::shared_ptr<http::client_request> req;
	std::shared_ptr<http::client_response> res;
	std::tie(req, res) = client->create_transaction();

	auto host = "gstatic.com";
	http::http_request preamble{};
	preamble.schema("schema");
	preamble.hostname(host);
	req->headers(std::move(preamble));
	req->end();
	static std::string body;
	res->on_body([]( auto res, auto char_array, auto len )
	{
		std::string lbod{ char_array.get(), len };
		body += lbod;
	});
	res->on_finished([&keep_alive, &server, &client](auto res) 
	{
		++finished;
		keep_alive.reset();
		server->close();
		client->close();
		LOGTRACE("Finished: ", res->preamble().serialize()  );
	});
	
	io.run();
	
	EXPECT_EQ( finished, 1 );
	EXPECT_EQ( headers, 1 );
	EXPECT_EQ( body.size(), real_size );
	EXPECT_EQ( real_body, body );
}

TEST(http2_session_client_test, frame_size_overflow)
{
	static int headers{0};
	static int finished{0};
	static std::size_t real_size{0};
	static std::string real_body{};

	std::function<void(std::string)> wcb_c;
	std::function<void(std::string)> wcb_s;
	boost::asio::io_service io;
	auto keep_alive = std::make_unique<boost::asio::io_service::work>(io);
	auto conn_c = std::make_shared<MockConnector>(io, wcb_c);
	auto conn_s = std::make_shared<MockConnector>(io, wcb_s);
	wcb_c = [&conn_s, &io](auto&& s)
	{
		io.post([&conn_s, s=std::move(s)] { conn_s->read(std::move(s)); });
	};
	wcb_s = [&conn_c, &io](auto&& s)
	{
		io.post([&conn_c, s=std::move(s)] { conn_c->read(std::move(s)); });
	};

	auto server = std::make_shared<http2::session>();
	server->connector(conn_s);
	conn_s->handler(server);
	server->on_request([](auto conn, auto req, auto res)
	{
		req->on_headers([](auto)
		{
			++headers;
		});
		req->on_finished([res=std::move(res)](auto&&)
		{
			static const std::string body{"Allright"};
			real_size = body.size();
			std::size_t multiplier = overflowing_frame_size( real_size );
			real_size *= multiplier;

			http::http_response r;
			r.protocol(http::proto_version::HTTP20);
			r.status(200);
			r.header("content-type", "text/plain");
			r.header("date", "Tue, 17 May 2016 14:53:09 GMT");
			r.hostname("doormat_app.org");
			r.content_len(real_size);
			res->headers(std::move(r));
			for ( std::size_t i = 0; i < multiplier; ++i )
				real_body += body;

			res->body( make_data_ptr( real_body ), real_size );
			res->end();
		});
	});

	auto client = std::make_shared<http2::session_client>();
	client->connector(conn_c);
	conn_c->handler(client);

	std::shared_ptr<http::client_request> req;
	std::shared_ptr<http::client_response> res;
	std::tie(req, res) = client->create_transaction();

	auto host = "gstatic.com";
	http::http_request preamble{};
	preamble.schema("schema");
	preamble.hostname(host);
	req->headers(std::move(preamble));
	req->end();
	static std::string body;
	res->on_body([]( auto res, auto char_array, auto len )
	{
		std::string lbod{ char_array.get(), len };
		body += lbod;
	});
	res->on_finished([&keep_alive, &server, &client](auto res)
	{
		++finished;
		keep_alive.reset();
		server->close();
		client->close();
		LOGTRACE("Finished: ", res->preamble().serialize()  );
	});

	io.run();

	EXPECT_EQ( finished, 1 );
	EXPECT_EQ( headers, 1 );
	EXPECT_EQ( body.size(), real_size );
	EXPECT_EQ( real_body, body );
}

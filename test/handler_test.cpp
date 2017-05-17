//#include <gtest/gtest.h>
//#include <gmock/gmock.h>

//#include <string>
//#include <array>
//#include <boost/asio.hpp>
//#include <boost/asio/ssl.hpp>

//#include "../src/service_locator/service_initializer.h"
//#include "../src/protocol/handler_factory.h"
//#include "../src/protocol/handler_http1.h"
//#include "../src/network/cloudia_pool.h"
//#include "../src/http/server/server_traits.h"
//#include "../src/http/server/request.h"
//#include "../src/http/server/response.h"
//#include "../src/http/server/server_connection.h"
//#include "mocks/mock_connector.h"

//#include "../src/dummy_node.h"

//namespace
//{

//struct HandlerTest: public ::testing::Test
//{
//	std::unique_ptr<boost::asio::deadline_timer> deadline;
//	MockConnector::wcb cb;
//	std::shared_ptr<MockConnector> conn;
//	std::string response;
//	boost::asio::io_service::work *keep_alive = nullptr;

//protected:
//    HandlerTest() : conn{std::make_shared<MockConnector>(cb)}
//	{
//        deadline.reset(new boost::asio::deadline_timer(conn->io_service()));
//        keep_alive = new boost::asio::io_service::work(conn->io_service());
//	}

//	virtual void SetUp()
//	{
//		deadline->expires_from_now(boost::posix_time::seconds(5));
//		deadline->async_wait([this](const boost::system::error_code& ec)
//		{
//			size_t missinghandlers = 1;
//			while(missinghandlers)
//			{
//				missinghandlers =conn->io_service().poll_one();
//			}
//			delete keep_alive;
//			conn->io_service().stop();
//		});
//	}

//	virtual void TearDown()
//	{
//		ASSERT_TRUE( conn->io_service().stopped() );
//		conn->io_service().reset();
//	}
//};

//}


//TEST_F(HandlerTest, http1_persistent)
//{
//	std::string req =
//		"GET / HTTP/1.1\r\n"
//		"host:localhost:1443\r\n"
//		"date: Tue, 17 May 2016 14:53:09 GMT\r\n"
//		"\r\n";

//	auto h1 = std::make_shared<server::handler_http1<http::server_traits>>(http::proto_version::HTTP11);
//	h1->connector(conn);
//	std::shared_ptr<http::server_connection> user_connection = h1;
//    user_connection->set_persistent(true);
//    user_connection->on_request([&](auto conn, auto req, auto res){
//        req->on_finished([res](auto req){
//            http::http_response r;
//            r.protocol(http::proto_version::HTTP11);
//            std::string body{"Ave client, dummy node says hello"};
//            r.status(200);
//            r.keepalive(true);
//            r.header("content-type", "text/plain");
//            r.header("date", "Tue, 17 May 2016 14:53:09 GMT");
//            r.content_len(body.size());
//            res->headers(std::move(r));
//            res->body(dstring{body.c_str(), body.size()});
//            res->end();
//        });
//    });


//	// list of chunks that I expect to receive inside the on_write()
//	std::string expected_response = "HTTP/1.1 200 OK\r\n"
//		"connection: keep-alive\r\n"
//		"content-length: 33\r\n"
//		"content-type: text/plain\r\n"
//		"date: Tue, 17 May 2016 14:53:09 GMT\r\n"
//		"\r\n"
//		"Ave client, dummy node says hello";

//	bool simulate_connection_closed = false;
//    boost::asio::deadline_timer t{conn->io_service()};
//	t.expires_from_now(boost::posix_time::seconds(2));
//	t.async_wait([&simulate_connection_closed](const boost::system::error_code &ec){simulate_connection_closed=true;});

//    cb = [this, &h1, &simulate_connection_closed](dstring chunk)
//	{
//        if(conn->io_service().stopped())
//			return;

//		while(h1->on_write(chunk))
//		{
//			if(h1->should_stop() || chunk.empty())
//			{
//				if(h1->should_stop())
//					deadline->cancel();
//				break;
//			}

//			response.append(chunk);
//			chunk = {};
//		}

//	};

//	ASSERT_TRUE(h1->start());
//	h1->on_read(req.data(), req.size());
//    conn->io_service().run();
//	EXPECT_EQ(expected_response, response);
//}


//TEST_F(HandlerTest, http1_non_persistent)
//{
//	std::string req =
//			"GET / HTTP/1.1\r\n"
//			"host:localhost:1443\r\n"
//			"date: Tue, 17 May 2016 14:53:09 GMT\r\n"
//			"connection: close\r\n"
//			"\r\n";

//	auto h1= std::make_shared<server::handler_http1<http::server_traits>>(http::proto_version::HTTP11);
//	h1->connector(conn);
//	std::shared_ptr<http::server_connection> user_connection = h1;
//    user_connection->set_persistent(false);
//    user_connection->on_request([&](auto conn, auto req, auto res){
//        req->on_finished([res](auto req){
//            http::http_response r;
//            r.protocol(http::proto_version::HTTP11);
//            std::string body{"Ave client, dummy node says hello"};
//            r.status(200);
//            r.keepalive(false);
//            r.header("content-type", "text/plain");
//            r.header("date", "Tue, 17 May 2016 14:53:09 GMT");
//            r.content_len(body.size());
//            res->headers(std::move(r));
//            res->body(dstring{body.c_str(), body.size()});
//            res->end();
//        });
//    });

//	// list of chunks that I expect to receive inside the on_write()
//	std::string expected_response = "HTTP/1.1 200 OK\r\n"
//			"connection: close\r\n"
//			"content-length: 33\r\n"
//			"content-type: text/plain\r\n"
//			"date: Tue, 17 May 2016 14:53:09 GMT\r\n"
//			"\r\n"
//			"Ave client, dummy node says hello";

//    bool simulate_connection_closed = false;
//    boost::asio::deadline_timer t{conn->io_service()};
//    t.expires_from_now(boost::posix_time::seconds(2));
//    t.async_wait([&simulate_connection_closed](const boost::system::error_code &ec){simulate_connection_closed=true;});
//    cb = [this, &h1](dstring chunk)
//	{
//        if(conn->io_service().stopped())
//			return;

//		while(h1->on_write(chunk))
//		{
//			if(h1->should_stop() || chunk.empty())
//			{
//				if(h1->should_stop())
//					deadline->cancel();
//				break;
//			}

//			response.append(chunk);
//			chunk = {};
//		}
//	};

//	ASSERT_TRUE(h1->start());
//	h1->on_read(req.data(), req.size());
//    conn->io_service().run();
//	ASSERT_TRUE(h1->should_stop());
//	EXPECT_EQ(expected_response, response);
//}


//TEST_F(HandlerTest, http1_pipelining)
//{

//	std::string req =
//			"GET / HTTP/1.1\r\n"
//			"host:localhost:1443\r\n"
//			"date: Tue, 17 May 2016 14:53:09 GMT\r\n"
//			"\r\n"
//			"GET / HTTP/1.1\r\n"
//			"host:localhost:1443\r\n"
//			"date: Tue, 17 May 2016 14:53:09 GMT\r\n"
//			"connection: close\r\n"
//			"\r\n";

//    auto h1= std::make_shared<server::handler_http1<http::server_traits>>(http::proto_version::HTTP11);
//	h1->connector(conn);
//	std::shared_ptr<http::server_connection> user_connection = h1;
//    user_connection->set_persistent(true);
//    user_connection->on_request([&](auto conn, auto req, auto res){
//        req->on_finished([res](auto req){
//	        bool keep_alive = req->preamble().keepalive();
//            http::http_response r;
//            r.protocol(http::proto_version::HTTP11);
//            std::string body{"Ave client, dummy node says hello"};
//            r.status(200);
//            r.keepalive(keep_alive);
//            r.header("content-type", "text/plain");
//            r.header("date", "Tue, 17 May 2016 14:53:09 GMT");
//            r.content_len(body.size());
//            res->headers(std::move(r));
//            res->body(dstring{body.c_str(), body.size()});
//            res->end();
//        });
//    });

//	// list of chunks that I expect to receive inside the on_write()
//    std::string expected_response =
//            "HTTP/1.1 200 OK\r\n"
//			"connection: keep-alive\r\n"
//			"content-length: 33\r\n"
//			"content-type: text/plain\r\n"
//			"date: Tue, 17 May 2016 14:53:09 GMT\r\n"

//			"\r\n"
//			"Ave client, dummy node says hello"
//			"HTTP/1.1 200 OK\r\n"
//			"connection: close\r\n"
//			"content-length: 33\r\n"
//			"content-type: text/plain\r\n"
//			"date: Tue, 17 May 2016 14:53:09 GMT\r\n"
//			"\r\n"
//			"Ave client, dummy node says hello";

//    bool simulate_connection_closed = false;
//    boost::asio::deadline_timer t{conn->io_service()};
//    t.expires_from_now(boost::posix_time::seconds(2));
//    t.async_wait([&simulate_connection_closed](const boost::system::error_code &ec){simulate_connection_closed=true;});

//    cb = [this, &h1, &simulate_connection_closed](dstring chunk)
//    {
//        if(conn->io_service().stopped())
//            return;

//        while(h1->on_write(chunk))
//        {
//            if(h1->should_stop() || chunk.empty())
//            {
//                if(h1->should_stop())
//                    deadline->cancel();
//                break;
//            }

//            response.append(chunk);
//            chunk = {};
//        }
//    };

//	ASSERT_TRUE(h1->start());
//	h1->on_read(req.data(), req.size());
//    conn->io_service().run();
//    ASSERT_TRUE(h1->should_stop());
//	EXPECT_EQ(expected_response, response);
//}

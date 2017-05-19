#include "src/http/client/request.h"
#include "src/http/client/response.h"
#include "src/http/client/client_connection.h"
#include "../src/requests_manager/client_wrapper.h"
#include "src/http/http_codec.h"
#include "common.h"
#include "mock_server/mock_server.h"
#include <functional>

namespace
{

dstring host{"provaprova.com"};
dstring path{"/static/ciaone/"};
std::string body{"000000000000"};
uint16_t port = 8454;

using namespace test_utils;
struct client_wrapper_test : public ::testing::Test
{
	std::string stream;
	http::http_request received_request;
	std::string received_request_body;

	http::http_codec decoder;
	std::unique_ptr<mock_server<>> server;
	std::unique_ptr<node_interface> ch;

	std::function<void(std::string)> read_fn;
	std::function<bool(void)> stop_condition;
	std::function<void(void)> thread_local_evaluator;

	client_wrapper_test()
	{
		decoder.register_callback(
			[this](http::http_structured_data** dest){*dest = &received_request;},
			[this](){},
			[this](dstring&& ch){received_request_body.append(ch);},
			[this](dstring&&, dstring&&){},
			[this](){},
			[this](int, bool&){FAIL() << "Decoding has failed. The message has been corrupted by client wrapper."; }
		);

		read_fn = [this](std::string bytes)
		{
			stream.append(bytes);
			if(stop_condition())
			{
				LOGTRACE("STOPPING");
				decoder.decode(stream.data(), stream.size());
				/*if(ch)
					ch->on_request_canceled(INTERNAL_ERROR_LONG(errors::http_error_code::request_timeout));*/
				//service::locator::socket_pool().stop();
				server->stop();
				service::locator::service_pool().allow_graceful_termination();
			}
			else
			{
				server->read(1, read_fn );
			}
		};
	}

protected:
	virtual void SetUp() override
	{
		preset::setup(new preset::mock_conf{});

		first_node::res_stop_fn = [this]()
		{
			if(thread_local_evaluator)
				thread_local_evaluator();
			service::locator::service_pool().get_thread_io_service()
				.post([this](){ch.reset();});
			server->stop();
		};
	}

	virtual void TearDown() override
	{
		preset::teardown(ch);
		stream.clear();
		received_request = {};
		received_request_body.clear();
	}
};

//TEST_F(client_wrapper_test, preamble_only)
//{
//	//test case to validate that client_wrapper does not corrupt the data.
//	http::http_request submitted_req;
//	submitted_req.protocol(http::proto_version::HTTP11);
//	submitted_req.method("GET");
//	submitted_req.hostname(host);
//	submitted_req.path(path);
//	submitted_req.keepalive(false);
//	submitted_req.setParameter("hostname", "::");
//	submitted_req.setParameter("port", std::to_string(port));

//	auto d1 = submitted_req.serialize();
//	const auto expected_bytes = d1.size();

//	auto init_fn = [this, submitted_req, expected_bytes](boost::asio::io_service& ios) mutable
//	{
//		preset::init_thread_local();
//		ch = make_unique_chain<node_interface, first_node, nodes::client_wrapper, blocked_node>();
//		stop_condition = [this](){ return true; };
//		server.reset( new mock_server{port} );
//		server->start([this, expected_bytes](){ server->read( expected_bytes, read_fn ); });
//		ch->on_request_preamble( std::move(submitted_req) );

//	};

//	service::locator::service_pool().run(init_fn);

//	auto d2 = received_request.serialize();
//	EXPECT_EQ(std::string(d1), std::string(d2));
//	EXPECT_TRUE(received_request.path() == path);
//	EXPECT_TRUE(received_request.urihost() == host);
//	EXPECT_TRUE(received_request.hostname() == host);
//}

TEST_F(client_wrapper_test, custom_destination_fail)
{
	dstring fake_port = 9879;
	//test case to validate that client_wrapper does not corrupt the data.
	http::http_request submitted_req;
	submitted_req.protocol(http::proto_version::HTTP11);
	submitted_req.method("GET");
	submitted_req.hostname(host);
	submitted_req.path(path);
	submitted_req.keepalive(false);
	submitted_req.header(http::hf_cyn_dest, "127.0.0.1");
	submitted_req.header(http::hf_cyn_dest_port, fake_port);
	submitted_req.setParameter("hostname", "::");
	submitted_req.setParameter("port", std::string(fake_port));
	boost::asio::deadline_timer dt{service::locator::service_pool().get_thread_io_service()};
	auto init_fn = [this, submitted_req, &dt](boost::asio::io_service& ios) mutable
	{
		preset::init_thread_local();
		ch = make_unique_chain<node_interface, first_node, nodes::client_wrapper, blocked_node>();
		stop_condition = [this](){ throw std::invalid_argument("shit"); return true; };
		server.reset( new mock_server<>{port} );
		server->start([this](){ server->read( 1, read_fn ); });
		ch->on_request_preamble( std::move(submitted_req) );
		dt.expires_from_now(boost::posix_time::seconds{1});
		dt.async_wait([this](const boost::system::error_code &ec){
			//service::locator::socket_pool().stop();
			if(server) server->stop();
			service::locator::service_pool().allow_graceful_termination();

		});
	};
	EXPECT_NO_THROW(service::locator::service_pool().run(init_fn));
}

//TEST_F(client_wrapper_test, custom_destination_ok)
//{
//	//test case to validate that client_wrapper does not corrupt the data.
//	http::http_request submitted_req;
//	submitted_req.protocol(http::proto_version::HTTP11);
//	submitted_req.method("GET");
//	submitted_req.hostname(host);
//	submitted_req.path(path);
//	submitted_req.keepalive(false);
//	submitted_req.header(http::hf_cyn_dest, "127.0.0.1");
//	submitted_req.header(http::hf_cyn_dest_port, "8454");
//	submitted_req.setParameter("hostname", "::");
//	submitted_req.setParameter("port", std::to_string(port));
//	auto d1 = submitted_req.serialize();
//	const auto expected_bytes = d1.size();
//	auto init_fn = [this, submitted_req, expected_bytes](boost::asio::io_service& ios) mutable
//	{
//	preset::init_thread_local();
//		ch = make_unique_chain<node_interface, first_node, nodes::client_wrapper, blocked_node>();
//		stop_condition = [this](){ return true; };
//		server.reset( new mock_server{port} );
//		server->start([this, expected_bytes](){ server->read( expected_bytes, read_fn ); });
//		ch->on_request_preamble( std::move(submitted_req) );
//	};
//	service::locator::service_pool().run(init_fn);

//	auto d2 = received_request.serialize();
//	EXPECT_EQ(std::string(d1), std::string(d2));
//	EXPECT_TRUE(received_request.path() == path);
//	EXPECT_TRUE(received_request.urihost() == host);
//	EXPECT_TRUE(received_request.hostname() == host);
//}

//TEST_F(client_wrapper_test, complete_request)
//{
//	http::http_request submitted_req;
//	submitted_req.protocol(http::proto_version::HTTP11);
//	submitted_req.method("PUT");
//	submitted_req.hostname(host);
//	submitted_req.path(path);
//	submitted_req.keepalive(false);
//	submitted_req.content_len(body.size());
//	submitted_req.setParameter("hostname", "::");
//	submitted_req.setParameter("port", std::to_string(port));

//	auto d1 = submitted_req.serialize();
//	const auto expected_bytes = d1.size() + body.size();

//	dstring body_chunk{body.c_str()};
//	auto init_fn = [this, submitted_req, body_chunk, expected_bytes](boost::asio::io_service& ios) mutable
//	{
//		preset::init_thread_local();
//		ch = make_unique_chain<node_interface, first_node, nodes::client_wrapper, blocked_node>();
//		stop_condition = [](){ return true; };

//		server.reset( new mock_server{port} );
//		server->start([this, expected_bytes](){server->read(expected_bytes, read_fn );});

//		ch->on_request_preamble(std::move(submitted_req));
//		ch->on_request_body(std::move(body_chunk));
//	};

//	service::locator::service_pool().run(init_fn);

//	auto d2 = received_request.serialize();

//	EXPECT_EQ(std::string(d1), std::string(d2));
//	EXPECT_TRUE(received_request.path() == path);
//	EXPECT_TRUE(received_request.urihost() == host);
//	EXPECT_TRUE(received_request.hostname() == host);
//	EXPECT_TRUE(received_request_body == body);
//}

//TEST_F(client_wrapper_test, no_response)
//{

//	http::http_request submitted_req;
//	submitted_req.protocol(http::proto_version::HTTP11);
//	submitted_req.method("GET");
//	submitted_req.hostname(host);
//	submitted_req.path(path);
//	submitted_req.keepalive(false);
//	submitted_req.setParameter("hostname", "::");
//	submitted_req.setParameter("port", std::to_string(port));

//	auto init_fn = [this, submitted_req](boost::asio::io_service& ios) mutable
//	{
//		preset::init_thread_local();
//		ch = make_unique_chain<node_interface, first_node, nodes::client_wrapper, blocked_node>();
//		stop_condition = [this](){
//			return server->is_stopped();
//		};

//		thread_local_evaluator = [this]()
//		{
//			EXPECT_TRUE( first_node::err == INTERNAL_ERROR(errors::http_error_code::internal_server_error) ) << first_node::err.code();
//		};

//		server.reset( new mock_server{port} );

//		server->start([this]()
//		{
//			server->read(1, read_fn );
//			//service::locator::socket_pool().stop();
//			service::locator::service_pool().allow_graceful_termination();
//		});

//		ch->on_request_preamble(std::move(submitted_req));
//		ch->on_request_finished();
//	};

//	service::locator::service_pool().run(init_fn);
//}

TEST_F(client_wrapper_test, request_response)
{
	http::http_request submitted_req;
	submitted_req.protocol(http::proto_version::HTTP11);
	submitted_req.method("PUT");
	submitted_req.hostname(host);
	submitted_req.path(path);
	submitted_req.keepalive(false);
	submitted_req.content_len(body.size());
	submitted_req.setParameter("hostname", "::");
	submitted_req.setParameter("port", std::to_string(port));

	auto req_d = submitted_req.serialize();
	const auto expected_bytes = req_d.size() + body.size();

	http::http_response submitted_res;
	submitted_res.protocol(http::proto_version::HTTP11);
	submitted_res.status(204);
	submitted_res.keepalive(false);

	auto res_d = submitted_res.serialize();

	dstring body_chunk{body.c_str()};
	auto init_fn = [this, submitted_req, body_chunk, expected_bytes, &res_d](boost::asio::io_service& ios) mutable
	{
		preset::init_thread_local();
		ch = make_unique_chain<node_interface, first_node, nodes::client_wrapper, blocked_node>();

		stop_condition = [this,res_d]()
		{
			server->write(res_d);
			return true;
		};

		thread_local_evaluator = [this, &res_d]()
		{
			ASSERT_TRUE(first_node::response.is_initialized());
			auto d = first_node::response->serialize();
			EXPECT_TRUE(d == res_d);
		};

		server.reset( new mock_server<>{port} );

		server->start([this, expected_bytes]()
		{
			server->read(expected_bytes, read_fn );
			//service::locator::socket_pool().stop();
		});

		ch->on_request_preamble(std::move(submitted_req));
		ch->on_request_body(std::move(body_chunk));
		ch->on_request_finished();
	};

	service::locator::service_pool().run(init_fn);

	auto d = received_request.serialize();

	EXPECT_EQ(std::string(req_d), std::string(d));
	EXPECT_TRUE(received_request.path() == path);
	EXPECT_TRUE(received_request.urihost() == host);
	EXPECT_TRUE(received_request.hostname() == host);
	EXPECT_TRUE(received_request_body == body);
}

//TEST_F(client_wrapper_test, client_hangs)
//{
//	//test if it indeed client_wrapper kills itself in case it hangs.
//	http::http_request submitted_req;
//	submitted_req.protocol(http::proto_version::HTTP11);
//	submitted_req.method("GET");
//	submitted_req.hostname(host);
//	submitted_req.path(path);
//	submitted_req.keepalive(false);
//	submitted_req.setParameter("hostname", "::");
//	submitted_req.setParameter("port", std::to_string(port));

//	auto init_fn = [this, submitted_req](boost::asio::io_service& ios) mutable
//	{
//		preset::init_thread_local();
//		ch = make_unique_chain<node_interface, first_node, nodes::client_wrapper, blocked_node>();
//		stop_condition = [this](){ return server->is_stopped(); };

//		thread_local_evaluator = [this]()
//		{
//			EXPECT_TRUE(first_node::err == INTERNAL_ERROR(errors::http_error_code::internal_server_error));
//		};

//		server.reset( new mock_server{port} );
//		server->start([this](){ server->read( 1, read_fn ); });

//		ch->on_request_preamble( std::move(submitted_req) );
//	};

//	service::locator::service_pool().run(init_fn);
//	EXPECT_GT(stream.size(), 0U);
//}

//TEST_F(client_wrapper_test, server_hangs)
//{
//	http::http_request submitted_req;
//	submitted_req.protocol(http::proto_version::HTTP11);
//	submitted_req.method("PUT");
//	submitted_req.hostname(host);
//	submitted_req.path(path);
//	submitted_req.keepalive(false);
//	submitted_req.content_len(body.size());
//	submitted_req.setParameter("hostname", "::");
//	submitted_req.setParameter("port", std::to_string(port));

//	auto req_d = submitted_req.serialize();

//	http::http_response submitted_res;
//	submitted_res.protocol(http::proto_version::HTTP11);
//	submitted_res.status(204);
//	submitted_req.keepalive(false);

//	auto res_d = submitted_res.serialize();

//	dstring body_chunk{body.c_str()};
//	auto init_fn = [this, submitted_req, body_chunk, &res_d](boost::asio::io_service& ios) mutable
//	{
//		preset::init_thread_local();
//		ch = make_unique_chain<node_interface, first_node, nodes::client_wrapper, blocked_node>();

//		stop_condition = [this](){return server->is_stopped();};

//		thread_local_evaluator = [this]()
//		{
//			EXPECT_TRUE(first_node::err == INTERNAL_ERROR(errors::http_error_code::internal_server_error));
//		};

//		first_node::res_start_fn = [this, &res_d]()
//		{
//			server->write(res_d.substr(0, res_d.size()/2));
//		};

//		server.reset( new mock_server{port} );
//		server->start([this]()
//		{
//			server->read(1, read_fn );
//			//service::locator::socket_pool().stop();
//		});

//		ch->on_request_preamble(std::move(submitted_req));
//		ch->on_request_body(std::move(body_chunk));
//		ch->on_request_finished();
//	};

//	service::locator::service_pool().run(init_fn);
//	EXPECT_GT(stream.size(), 0U);
//}

}

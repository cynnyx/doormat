#include "../src/connector.h"
#include "mocks/mock_server/mock_server.h"
#include "mocks/mock_handler/mock_handler.h"
#include <gtest/gtest.h>
#include <gmock/gmock.h>


TEST(connector_test, timeout) {
	boost::asio::io_service io_service;

	mock_server<> server{io_service};
	server.start([]{});

	boost::asio::ip::tcp::endpoint endpoint{boost::asio::ip::address::from_string("127.0.0.1"), 8454U};
	auto socket = std::make_shared<boost::asio::ip::tcp::socket>(io_service);
	std::shared_ptr<server::connector<server::tcp_socket>> connector{nullptr};
	bool timeout_called{false};

	auto timeout_cb = [&](){
		if(!connector)
		{
			FAIL() << "Connector should not be null when the timeout callback is executed.";
		}

		if(timeout_called)
		{
			FAIL() << "Timeout has already been called";
		}

		timeout_called = true;
		connector->stop();
		server.stop();
	};

	auto _handler = std::make_shared<mock_handler>(timeout_cb);

	socket->async_connect(endpoint, [socket, &connector, &_handler](auto error) {
		if(error)
		{
			FAIL() << "Error while connect";
			return;
		}

		connector = std::make_shared<server::connector<server::tcp_socket>>(std::move(socket));
		connector->handler(_handler);
		_handler->connector(connector);
		connector->set_timeout(std::chrono::milliseconds{100});
		connector->start();
	});

	io_service.run();
	ASSERT_EQ(connector.use_count(), 1U);
	ASSERT_TRUE(timeout_called);
	ASSERT_TRUE(server.is_stopped());
}


TEST(connector_test, write_success) {
	boost::asio::io_service io_service;

	mock_server<> server{io_service};
	server.start([]{});

	boost::asio::ip::tcp::endpoint endpoint{boost::asio::ip::address::from_string("127.0.0.1"), 8454U};
	auto socket = std::make_shared<boost::asio::ip::tcp::socket>(io_service);
	std::shared_ptr<server::connector<server::tcp_socket>> connector{nullptr};

	bool success_cb_called{false};
	bool error_cb_called{false};

	auto success_cb = [&](){
		if(!connector)
		{
			FAIL() << "Connector should not be null when the timeout callback is executed.";
		}

		success_cb_called = true;
		connector->stop();
		server.stop();
	};

	auto error_cb = [&](){
		if(!connector)
		{
			FAIL() << "Connector should not be null when the timeout callback is executed.";
		}

		error_cb_called = true;
		connector->stop();
		server.stop();
	};

	mock_handler::success_or_error_collbacks cbs{};
	cbs.push_back(std::make_pair(success_cb, error_cb));
	auto _handler = std::make_shared<mock_handler>([](){}, cbs);

	socket->async_connect(endpoint, [socket, &connector, &_handler](auto error) {
		if(error)
		{
			FAIL() << "Error while connect";
			return;
		}

		connector = std::make_shared<server::connector<server::tcp_socket>>(std::move(socket));
		connector->handler(_handler);
		_handler->connector(connector);
		connector->start();
		connector->do_write();
	});

	io_service.run();
	ASSERT_EQ(connector.use_count(), 1U);
	ASSERT_TRUE(success_cb_called);
	ASSERT_FALSE(error_cb_called);
	ASSERT_TRUE(server.is_stopped());
}


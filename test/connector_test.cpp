#include "../src/connector.h"
#include "mocks/mock_server/mock_server.h"
#include "mocks/mock_handler/mock_handler.h"
#include <gtest/gtest.h>
#include <gmock/gmock.h>


TEST(connector_test, timer) {
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
		std::cout << "you called me!" << std::endl;
		timeout_called = true;
		connector->stop();
		server.stop();

	};

	socket->async_connect(endpoint, [socket, &connector, &timeout_cb](auto error) {
		if(error)
		{
			FAIL() << "Error while connect";
			return;
		}
		connector = std::make_shared<server::connector<server::tcp_socket>>(std::move(socket));
		auto _handler = std::make_shared<mock_handler>(timeout_cb);
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




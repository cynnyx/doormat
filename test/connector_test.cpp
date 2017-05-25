#include "../src/connector.h"
#include "mocks/mock_server/mock_server.h"
#include "mocks/mock_handler/mock_handler.h"
#include <gtest/gtest.h>
#include <gmock/gmock.h>


TEST(connector_test, timer) {
	boost::asio::io_service io_service;

	mock_server<> server{io_service};
	server.start([]{});

	auto socket = std::make_shared<boost::asio::ip::tcp::socket>(io_service);

	boost::asio::ip::tcp::endpoint endpoint{boost::asio::ip::address::from_string("127.0.0.1"), 8454U};
	socket->async_connect(endpoint, [](auto error) {
		std::cout << error << std::endl;
	});

	auto connector = std::make_shared<server::connector<server::tcp_socket>>(std::move(socket));
	auto _handler = std::make_shared<mock_handler>();

	connector->handler(_handler);
	_handler->connector(connector);

	connector->do_write();

	io_service.run();

	server.stop();
	ASSERT_TRUE(server.is_stopped());
}




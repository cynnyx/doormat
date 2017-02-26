#include <gtest/gtest.h>
#include <nodes/common.h>
#include <mock_server/mock_server.h>
#include "../../src/network/socket_pool.h"

using namespace test_utils;

struct socket_pool_config : public preset::mock_conf
{
	std::string get_route_map() const noexcept override
	{
		static constexpr auto rm = "./etc/doormat/route_map_sockets";
		return rm;
	}
};

struct socket_pool_test : public ::testing::Test
{
	std::vector<mock_server> servers;
	std::function<void()> server_starting;
	std::function<void(boost::asio::io_service &service)> init_function;
	std::function<void()> testing_function;
	std::unique_ptr<network::socket_pool> socket_pool{nullptr};
	bool stopping = false;
protected:
	virtual void SetUp() override
	{
		preset::setup(new socket_pool_config{}); //create new mock conf dependant on the number of boards that we want to test.  This is not funny at all.
		uint16_t starting_port = 8454;
		for(int i = 0; i < 30; ++i)
		{
			servers.emplace_back(starting_port++);
		}

		server_starting = [this](){};
	}

	virtual void TearDown() override
	{
		for(auto &s: servers)  s.stop();
		preset::teardown();
	}
};


TEST_F(socket_pool_test, all_positive_connections)
{
	int count = 0;
	init_function = [this, &count](boost::asio::io_service &service) mutable
	{
		for(auto &s: servers) s.start([&s](){ });
		socket_pool = std::unique_ptr<network::socket_pool>{new network::socket_pool(1)};
		for(int i = 0; i < 10; ++i)
		{
			socket_pool->get_socket([this, &count](std::unique_ptr<boost::asio::ip::tcp::socket> socket)
			{
				ASSERT_TRUE(socket);
				socket->close();
				++count;
				if(count == 10)
				{
					service::locator::service_pool().allow_graceful_termination();
					stopping = true;
					socket_pool->stop();
					for(auto &s: servers) s.stop();
				}
			});
		}
	};
	service::locator::service_pool().run(init_function);
	ASSERT_EQ(count, 10);
}



TEST_F(socket_pool_test, could_not_get_new_socket)
{
	int count = 0;
	init_function = [this, &count](boost::asio::io_service &io) mutable
	{
		servers[0].start(server_starting, true); //start just one server, so that the second request will fail.
		socket_pool = std::unique_ptr<network::socket_pool>{new network::socket_pool(1)};
		socket_pool->get_socket([this, &count](std::unique_ptr<boost::asio::ip::tcp::socket> socket){++count;});
		socket_pool->get_socket([this, &count](std::unique_ptr<boost::asio::ip::tcp::socket> socket)
		{
			ASSERT_FALSE(socket);
			++count;
			service::locator::service_pool().allow_graceful_termination();
			for(auto &server: servers) server.stop();
			socket_pool->stop();
		});
	};
	service::locator::service_pool().run(init_function);
	ASSERT_EQ(count, 2);
}

TEST_F(socket_pool_test, without_servers)
{
	int count = 0;
	init_function = [this, &count](boost::asio::io_service &io) mutable
	{
		servers[0].stop(); //start just one server, so that the second request will fail.
		socket_pool = std::unique_ptr<network::socket_pool>{new network::socket_pool(1)};
		socket_pool->get_socket([this, &count](std::unique_ptr<boost::asio::ip::tcp::socket> socket)
		{
			ASSERT_FALSE(socket);
			++count;
			service::locator::service_pool().allow_graceful_termination();
			socket_pool->stop();
		});


	};
	service::locator::service_pool().run(init_function);
	ASSERT_EQ(count, 1);
}

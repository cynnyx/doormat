#include <gtest/gtest.h>
#include <mock_server/mock_server.h>
#include <nodes/common.h>
#include "src/network/communicator/communicator.h"

/*
using namespace test_utils;

struct communicator_test : public ::testing::Test
{
	mock_server<> server;
	std::function<void(boost::asio::io_service &service)> init_function;
	std::unique_ptr<network::communicator> c{nullptr};
	int count = 0; //should be 4
protected:
	virtual void SetUp() override
	{
		preset::setup(new preset::mock_conf{});
	}
	virtual void TearDown() override
	{
		preset::teardown();
	}
};



TEST_F(communicator_test, echo_once)
{

	std::string echo_string{"ciao"};
	init_function = [this, &echo_string](boost::asio::io_service &service) mutable
	{

		server.start([this, &echo_string]()
		{
			++count;
			server.read(echo_string.size(), [this, &echo_string](std::string readed){
				ASSERT_TRUE((echo_string == readed));
				++count;
				server.write(echo_string, [this, &echo_string](size_t sz){
					ASSERT_EQ(sz, echo_string.size());
					server.stop();
					++count;
				});
			});

		});

		preset::init_thread_local();

		service::locator::socket_pool().get_socket([this, &echo_string](std::unique_ptr<boost::asio::ip::tcp::socket> socket){
			auto com_ptr = new network::communicator(std::move(socket),
			[this, &echo_string](const char * data, size_t size){
				std::cout << "readed" << std::endl;
				ASSERT_TRUE((std::string(data, size) == echo_string));
				++count;
				service::locator::service_pool().allow_graceful_termination();
				service::locator::socket_pool().stop();
				c->stop();
			},
			[this](errors::error_code ec){
				ASSERT_TRUE(ec.code() == 1);
			});

			c = std::unique_ptr<network::communicator>(com_ptr);
			c->start();
			c->write(dstring{echo_string.c_str()});
		});
	};
	service::locator::service_pool().run(init_function);
	ASSERT_EQ(count, 4);
}


TEST_F(communicator_test, echo_twice)
{
	std::string echo_string{"0123456789"};
	init_function = [this, &echo_string](boost::asio::io_service &service) mutable
	{

		server.start([this, &echo_string]()
		{
			++count;
			server.read(echo_string.size(), [this, &echo_string](std::string readed){
				ASSERT_TRUE((echo_string == readed));
				++count;
				server.write(echo_string, [this, &echo_string](size_t sz){
					ASSERT_EQ(sz, echo_string.size());
					server.stop();
					++count;
				});
			});
		});

		preset::init_thread_local();

		service::locator::socket_pool().get_socket([this, &echo_string](std::unique_ptr<boost::asio::ip::tcp::socket> socket){
			auto com_ptr = new network::communicator(std::move(socket),
													 [this, &echo_string](const char * data, size_t size){
				//ASSERT_TRUE((std::string(data, size) == echo_string/2));
				std::cout << "readed " << std::string(data, size) << std::endl;
				++count;
				service::locator::service_pool().allow_graceful_termination();
				service::locator::socket_pool().stop();
				c->stop();
			},
			[this](errors::error_code ec){
					ASSERT_TRUE(ec.code() == 1);
		});

			c = std::unique_ptr<network::communicator>(com_ptr);
			c->start();
			c->write(dstring{echo_string.data(), echo_string.size()/2});
			c->write(dstring{echo_string.data()+echo_string.size()/2, echo_string.size()/2});
		});
	};
	service::locator::service_pool().run(init_function);
	ASSERT_EQ(count, 4);
}
*/

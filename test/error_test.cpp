#include <gtest/gtest.h>

#include "testcommon.h"

#include <memory>
/*
#include "../src/service_locator/service_initializer.h"
#include "../src/http/http_structured_data.h"
#include "../src/errors/error_messages.h"
#include "../src/fs/fs_manager_wrapper.h"

#include "boost/asio.hpp"

using namespace std;
using namespace errors;
using namespace http;
using namespace cynny::cynnypp::filesystem;


class ErrorTest : public ::testing::Test
{
public:
	ErrorTest()
	{
		service::initializer::load_configuration("./etc/doormat/doormat.test.config");
		service::initializer::init_services();
		service::locator::configuration().set_thread_number(1);
		service::initializer::set_fs_manager(new fs_manager_wrapper() );
	}

	// Test interface
protected:
	void SetUp() override
	{
		count = 0;
	}

	void TearDown() override
	{
		ASSERT_EQ(count, 1);
	}

	int count = 0;

};


TEST_F(ErrorTest, Forbidden)
{
	const auto path = service::locator::configuration().get_error_filename(static_cast<uint16_t>(http_error_code::forbidden));
	std::cout << "path is" << path << std::endl;
	error_body(path, [this](const ErrorCode& ec, std::string msg)
	{
		EXPECT_FALSE(ec);
		EXPECT_EQ(msg, forbidden_html);
		service::locator::service_pool().stop();
		++count;
	});

	service::locator::service_pool().run();
}


TEST_F(ErrorTest, MethodNotAllowed)
{
	const auto path = service::locator::configuration().get_error_filename(static_cast<uint16_t>(http_error_code::method_not_allowed));
	error_body(path, [this](const ErrorCode& ec, std::string msg)
	{
		EXPECT_FALSE(ec);
		EXPECT_EQ(msg, method_not_allowed_html);
		service::locator::service_pool().stop();
		++count;
	});

	service::locator::service_pool().run();
}


TEST_F(ErrorTest, RequestTimeout)
{
	const auto path = service::locator::configuration().get_error_filename(static_cast<uint16_t>(http_error_code::request_timeout));
	error_body(path, [this](const ErrorCode& ec, std::string msg)
	{
		EXPECT_FALSE(ec);
		EXPECT_EQ(msg, request_timeout_html);
		service::locator::service_pool().stop();

		++count;
	});

	service::locator::service_pool().run();


}


TEST_F(ErrorTest, InternalServerError)
{
	const auto path = service::locator::configuration().get_error_filename(static_cast<uint16_t>(http_error_code::internal_server_error));
	error_body(path, [this](const ErrorCode& ec, std::string msg)
	{
		EXPECT_FALSE(ec);
		EXPECT_EQ(msg, internal_server_error_html);
		service::locator::service_pool().stop();
		++count;
	});
	service::locator::service_pool().run();


}


TEST_F(ErrorTest, BadGateway)
{
	const auto path = service::locator::configuration().get_error_filename(static_cast<uint16_t>(http_error_code::bad_gateway));
	error_body(path, [this](const ErrorCode& ec, std::string msg)
	{
		EXPECT_FALSE(ec);
		EXPECT_EQ(msg, bad_gateway_html);
		service::locator::service_pool().stop();
		++count;
	});
	service::locator::service_pool().run();


}


TEST_F(ErrorTest, ServiceUnavailable)
{
	const auto path = service::locator::configuration().get_error_filename(static_cast<uint16_t>(http_error_code::service_unavailable));
	error_body(path, [this](const ErrorCode& ec, std::string msg)
	{
		EXPECT_FALSE(ec);
		EXPECT_EQ(msg, service_unavailable_html);
		service::locator::service_pool().stop();
		++count;
	});
	service::locator::service_pool().run();


}


TEST_F(ErrorTest, GatewayTimeout)
{
	const auto path = service::locator::configuration().get_error_filename(static_cast<uint16_t>(http_error_code::gateway_timeout));
	error_body(path, [this](const ErrorCode& ec, std::string msg)
	{
		EXPECT_FALSE(ec);
		EXPECT_EQ(msg, gateway_timeout_html);
		service::locator::service_pool().stop();
		++count;
	});
	service::locator::service_pool().run();


}*/


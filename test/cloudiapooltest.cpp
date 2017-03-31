#include <string>

#include <gtest/gtest.h>

#include "../src/service_locator/service_initializer.h"
#include "../src/service_locator/service_locator.h"
#include "../src/io_service_pool.h"
#include "../src/network/cloudia_pool.h"
#include "testcommon.h"

#include <iostream>
struct cloudiapooltest: public ::testing::Test
{
	class mockconf: public configuration::configuration_wrapper
	{
		uint64_t get_board_timeout() const noexcept override { return 3000; }
	};
	virtual void SetUp() 
	{
		service::initializer::set_service_pool( new server::io_service_pool( 1 ) );
		service::initializer::set_configuration( new mockconf() );
	}
	virtual void TearDown() 
	{
		
	}
};


TEST_F(cloudiapooltest, creation)
{
	network::cloudia_pool_factory cpf;
	auto cp = cpf.get_socket_factory();
	ASSERT_NE( nullptr, cp.get() );
}

TEST_F(cloudiapooltest, connection)
{
	auto&& ioservice = service::locator::service_pool().get_io_service();
	bool ok = false;
	network::cloudia_pool_factory cpf;
	auto cp = cpf.get_socket_factory();
	
	http::http_request req = make_request( http::proto_version::HTTP11, http_method::HTTP_GET, "www.google.it", "" );
	req.urihost("www.google.it");
	req.port("80");
	cp->get_socket( req, [&ok, &ioservice]
		( std::unique_ptr<network::socket_factory<boost::asio::ip::tcp::socket>::socket_type> s) mutable 
	{
		ASSERT_TRUE( static_cast<bool>( s ) );
		ok = true;

		ioservice.stop();
	});
	
	ioservice.run();
	ASSERT_TRUE( ok );
}

TEST_F(cloudiapooltest, error)
{
	auto&& ioservice = service::locator::service_pool().get_io_service();
	bool ok = true;
	network::cloudia_pool_factory cpf;
	auto cp = cpf.get_socket_factory();
	
	http::http_request req = make_request( http::proto_version::HTTP11, http_method::HTTP_GET, "www.google.it.sgranarok", "" );
	req.urihost("www.google.it.sgranarok");
	req.port("666");
	cp->get_socket( req, [&ioservice] ( std::unique_ptr<network::socket_factory<boost::asio::ip::tcp::socket>::socket_type> s) mutable 
	{
		ioservice.stop();
		FAIL();
	}, [&ok, &ioservice]() mutable { ok = false; ioservice.stop(); });
	
	ioservice.run();
	ASSERT_FALSE( ok );
}


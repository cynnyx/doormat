#include <gtest/gtest.h>
#include "../../../src/http/server/request.h"
#include "../../../src/http/server/response.h"
#include "../../mocks/mock_connector/mock_connector.h"
#include "../src/http2/session.h"
#include "../src/http2/stream.h"

#include <memory>
#include <utility>

// TODO this is a duplicate - bring it out
static std::unique_ptr<char[]> make_data_ptr(const std::string& s)
{
	auto ptr = std::make_unique<char[]>(s.size());
	std::copy(s.begin(), s.end(), ptr.get());
	return std::move(ptr);
}

using server_connection_t = http2::session;

class http2_test : public ::testing::Test
{
public:
	void SetUp()
	{
		_write_cb = [](dstring) {};
		mock_connector = std::make_shared<MockConnector>(io, _write_cb);
		_handler = std::make_shared<server_connection_t>();
		mock_connector->handler(_handler);
		response = "";
	}

	boost::asio::io_service io;
	std::shared_ptr<MockConnector> mock_connector;
	std::shared_ptr<server_connection_t> _handler;
	MockConnector::wcb _write_cb;
	std::string response;
};

TEST_F(http2_test, randomdata)
{
	// This should not throw!
	const char raw_request[] = 
		"\x00\x05\x06\x04\x00\x00\x00\x00\x00\x00\x03\x00\x00\x00\x64\x50"
		"\x52\x49\x20\x2A\x20\x48\x54\x54\x50\x2F\x32\x2E\x30\x0D\x0A\x0D"
		"\x00\x00\x00\x00\x00\x00\x00\x00";
	std::size_t len = sizeof( raw_request );
		
	_handler = std::make_shared<http2::session>();
	
	_handler->start();
	bool error = _handler->on_read(raw_request, len);
	ASSERT_FALSE( error );
}

TEST_F(http2_test, getting_200_at_once)
{
	_handler->on_request([&](auto conn, auto req, auto res) 
	{
		req->on_finished([res](auto req) 
		{
			http::http_response r;
			r.protocol(http::proto_version::HTTP20);
			std::string body{"Ave client, dummy node says hello"};
			r.status(200);
			r.header("content-type", "text/plain");
			r.header("date", "Tue, 17 May 2016 14:53:09 GMT");
			r.content_len(body.size());
			res->headers(std::move(r));
			res->body(make_data_ptr(body), body.size());
			res->end();
		});
	});
	
	constexpr const char raw_response[] =
		"\x00\x00\x00\x04\x01\x00\x00\x00"
		"\x00\x00\x00\x27\x01\x04\x00\x00"
		"\x00\x0d\x88\x0f\x0d\x02\x33\x33"
		"\x5f\x87\x49\x7c\xa5\x8a\xe8\x19"
		"\xaa\x61\x96\xdf\x69\x7e\x94\x0b"
		"\xaa\x68\x1f\xa5\x04\x00\xb8\xa0"
		"\x5a\xb8\xdb\x37\x00\xfa\x98\xb4"
		"\x6f\x00\x00\x21\x00\x00\x00\x00"
		"\x00\x0d\x41\x76\x65\x20\x63\x6c"
		"\x69\x65\x6e\x74\x2c\x20\x64\x75"
		"\x6d\x6d\x79\x20\x6e\x6f\x64\x65"
		"\x20\x73\x61\x79\x73\x20\x68\x65"
		"\x6c\x6c\x6f\x00\x00\x00\x00\x01"
		"\x00\x00\x00";

 	constexpr const std::size_t len_resp = sizeof ( raw_response );
	
	bool done{false};
	dstring actual_res;
	_write_cb = [&]( dstring w )
	{
		actual_res.append( w );

		if ( actual_res.size() == len_resp ) // Not the best way to do this
		{
			bool eq{true};
			for ( std::size_t i = 0; i < len_resp && eq; ++i )
				eq = actual_res.cdata()[i] == raw_response[i];
			done = true;
		}
	};
	
	mock_connector->io_service().post([this]() 
	{
		const char raw_request[] = 
			"\x50\x52\x49\x20\x2A\x20\x48\x54\x54\x50\x2F\x32\x2E\x30\x0D\x0A"
			"\x0D\x0A\x53\x4D\x0D\x0A\x0D\x0A\x00\x00\x0C\x04\x00\x00\x00\x00"
			"\x00\x00\x03\x00\x00\x00\x64\x00\x04\x00\x00\xFF\xFF\x00\x00\x05"
			"\x02\x00\x00\x00\x00\x03\x00\x00\x00\x00\xC8\x00\x00\x05\x02\x00"
			"\x00\x00\x00\x05\x00\x00\x00\x00\x64\x00\x00\x05\x02\x00\x00\x00"
			"\x00\x07\x00\x00\x00\x00\x00\x00\x00\x05\x02\x00\x00\x00\x00\x09"
			"\x00\x00\x00\x07\x00\x00\x00\x05\x02\x00\x00\x00\x00\x0B\x00\x00"
			"\x00\x03\x00\x00\x00\x26\x01\x25\x00\x00\x00\x0D\x00\x00\x00\x0B"
			"\x0F\x82\x84\x86\x41\x8A\xA0\xE4\x1D\x13\x9D\x09\xB8\xF3\xCF\x3D"
			"\x53\x03\x2A\x2F\x2A\x90\x7A\x8A\xAA\x69\xD2\x9A\xC4\xC0\x57\x08"
			"\x17\x07";
		std::size_t len = sizeof( raw_request );
		std::string pd{ raw_request, len };
		mock_connector->read(pd);
	});
	mock_connector->io_service().run();

	ASSERT_TRUE(done);	
	// TODO GO AWAY IS MISSING 
// 	ASSERT_TRUE(_handler->should_stop());

}

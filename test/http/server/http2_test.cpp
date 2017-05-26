#include <gtest/gtest.h>
#include "../../mocks/mock_connector/mock_connector.h"
#include "../src/http2/session.h"
#include "../src/http2/stream.h"

#include <memory>

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
		
	std::unique_ptr<http2::session> session = std::make_unique<http2::session>();
	
	session->start();
	bool error = session->on_read(raw_request, len);
	ASSERT_FALSE( error );
}

/*
 * Raw data in encoded request
 * 
 * send SETTINGS frame <length=12, flags=0x00, stream_id=0>
		(niv=2)
		[SETTINGS_MAX_CONCURRENT_STREAMS(0x03):100]
		[SETTINGS_INITIAL_WINDOW_SIZE(0x04):65535]
	send PRIORITY frame <length=5, flags=0x00, stream_id=3>
		(dep_stream_id=0, weight=201, exclusive=0)
	send PRIORITY frame <length=5, flags=0x00, stream_id=5>
		(dep_stream_id=0, weight=101, exclusive=0)
	send PRIORITY frame <length=5, flags=0x00, stream_id=7>
		(dep_stream_id=0, weight=1, exclusive=0)
	send PRIORITY frame <length=5, flags=0x00, stream_id=9>
		(dep_stream_id=7, weight=1, exclusive=0)
	send PRIORITY frame <length=5, flags=0x00, stream_id=11>
		(dep_stream_id=3, weight=1, exclusive=0)
	send HEADERS frame <length=38, flags=0x25, stream_id=13>
		; END_STREAM | END_HEADERS | PRIORITY
		(padlen=0, dep_stream_id=11, weight=16, exclusive=0)
		; Open new stream
		:method: GET
		:path: /
		:scheme: http
		:authority: localhost:8888
		accept: * / *
		accept-encoding: gzip, deflate
		user-agent: nghttp2/1.10.0
	recv SETTINGS frame <length=6, flags=0x00, stream_id=0>
		(niv=1)
		[SETTINGS_MAX_CONCURRENT_STREAMS(0x03):100]
	send SETTINGS frame <length=0, flags=0x01, stream_id=0>
		; ACK
		(niv=0)
	recv SETTINGS frame <length=0, flags=0x01, stream_id=0>
		; ACK
		(niv=0)
	recv (stream_id=13) :status: 404
	recv (stream_id=13) server: nghttpd nghttp2/1.10.0
	recv (stream_id=13) date: Thu, 25 May 2017 10:20:56 GMT
	recv (stream_id=13) content-type: text/html; charset=UTF-8
	recv HEADERS frame <length=63, flags=0x04, stream_id=13>
		; END_HEADERS
		(padlen=0)
		; First response header
<html><head><title>404 Not Found</title></head><body><h1>404 Not Found</h1><hr><address>nghttpd nghttp2/1.10.0 at port 8888</address></body></html>[  0.000] recv DATA frame <length=147, flags=0x01, stream_id=13>
		; END_STREAM
	send GOAWAY frame <length=8, flags=0x00, stream_id=0>
          (last_stream_id=0, error_code=NO_ERROR(0x00), opaque_data(0)=[])
 */
TEST_F(http2_test, getting_404_at_once)
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
	
	const char raw_response[] =
	    "\x00\x00\x06\x04\x00\x00\x00\x00\x00\x00\x03\x00\x00\x00\x64\x50"
		"\x52\x49\x20\x2A\x20\x48\x54\x54\x50\x2F\x32\x2E\x30\x0D\x0A\x0D"
		"\x0A\x53\x4D\x0D\x0A\x0D\x0A\x00\x00\x0C\x04\x00\x00\x00\x00\x00"
		"\x00\x03\x00\x00\x00\x64\x00\x04\x00\x00\xFF\xFF\x00\x00\x05\x02"
		"\x00\x00\x00\x00\x03\x00\x00\x00\x00\xC8\x00\x00\x05\x02\x00\x00"
		"\x00\x00\x05\x00\x00\x00\x00\x64\x00\x00\x05\x02\x00\x00\x00\x00"
		"\x07\x00\x00\x00\x00\x00\x00\x00\x05\x02\x00\x00\x00\x00\x09\x00"
		"\x00\x00\x07\x00\x00\x00\x05\x02\x00\x00\x00\x00\x0B\x00\x00\x00"
		"\x03\x00\x00\x00\x26\x01\x25\x00\x00\x00\x0D\x00\x00\x00\x0B\x0F"
		"\x82\x84\x86\x41\x8A\xA0\xE4\x1D\x13\x9D\x09\xB8\xF3\xCF\x3D\x53"
		"\x03\x2A\x2F\x2A\x90\x7A\x8A\xAA\x69\xD2\x9A\xC4\xC0\x57\x08\x17"
		"\x07\x00\x00\x00\x04\x01\x00\x00\x00\x00\x00\x00\x00\x04\x01\x00"
		"\x00\x00\x00\x00\x00\x3F\x01\x04\x00\x00\x00\x0D\x8D\x76\x90\xAA"
		"\x69\xD2\x9A\xE4\x52\xA9\xA7\x4A\x6B\x13\x01\x5C\x20\x5C\x1F\x61"
		"\x96\xDF\x3D\xBF\x4A\x09\xB5\x34\x0F\xD2\x82\x00\x5D\x50\x20\xB8"
		"\x20\x5C\x6D\xC5\x31\x68\xDF\x5F\x92\x49\x7C\xA5\x89\xD3\x4D\x1F"
		"\x6A\x12\x71\xD8\x82\xA6\x0E\x1B\xF0\xAC\xF7\x00\x00\x93\x00\x01"
		"\x00\x00\x00\x0D\x3C\x68\x74\x6D\x6C\x3E\x3C\x68\x65\x61\x64\x3E"
		"\x3C\x74\x69\x74\x6C\x65\x3E\x34\x30\x34\x20\x4E\x6F\x74\x20\x46"
		"\x6F\x75\x6E\x64\x3C\x2F\x74\x69\x74\x6C\x65\x3E\x3C\x2F\x68\x65"
		"\x61\x64\x3E\x3C\x62\x6F\x64\x79\x3E\x3C\x68\x31\x3E\x34\x30\x34"
		"\x20\x4E\x6F\x74\x20\x46\x6F\x75\x6E\x64\x3C\x2F\x68\x31\x3E\x3C"
		"\x68\x72\x3E\x3C\x61\x64\x64\x72\x65\x73\x73\x3E\x6E\x67\x68\x74"
		"\x74\x70\x64\x20\x6E\x67\x68\x74\x74\x70\x32\x2F\x31\x2E\x31\x30"
		"\x2E\x30\x20\x61\x74\x20\x70\x6F\x72\x74\x20\x38\x38\x38\x38\x3C"
		"\x2F\x61\x64\x64\x72\x65\x73\x73\x3E\x3C\x2F\x62\x6F\x64\x79\x3E"
		"\x3C\x2F\x68\x74\x6D\x6C\x3E\x00\x00\x08\x07\x00\x00\x00\x00\x00"
		"\x00\x00\x00\x00\x00\x00\x00\x00";

	std::size_t len_resp = sizeof ( raw_response );
	std::size_t cursor = 0;
		
	std::unique_ptr<http2::session> session = std::make_unique<http2::session>();
	
	session->start();
	bool r = session->on_read(raw_request, len);
	
	ASSERT_TRUE( r );
	while ( ! session->should_stop() )
	{
		if ( cursor >= len_resp )
			FAIL();
		std::string this_write;
		session->on_write( this_write );
		
		std::size_t tw_len = this_write.size();
		ASSERT_TRUE( tw_len < len_resp - cursor );
		int v = memcmp(raw_response + cursor, this_write.c_str(), tw_len );
		ASSERT_TRUE( v == 0 );
		cursor += tw_len;
	}
}








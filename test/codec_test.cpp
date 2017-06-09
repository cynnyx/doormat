#include <string>
#include <sstream>
#include <array>
#include <functional>
#include <vector>

#include <gtest/gtest.h>

#include "testcommon.h"

#include "../src/http/http_codec.h"
#include "../src/http/http_request.h"
#include "../src/http/http_response.h"

namespace
{

using namespace std;
using namespace http;

static const std::string schema{"http"};
static const std::string urihost{"localhost"};
static const std::string port{"2001"};
static const std::string path{"/static"};
static const std::string hostname{"localhost:2001"};
static const std::string date{"Tue, 17 May 2016 14:53:09 GMT"};
static const std::string cyndate{"1470328824"};
static const std::string transferencoding{"tranfer-encoding"};
static const std::string tranfertype{"whatever"};
static const std::string customheader{"custom-Head"};
static const std::string customval0{"123"};
static const std::string customval1{"212312"};
static const std::string customval2{"33"};
static const std::string query("abra=cadabra&topolino=minnie");

http_codec codec;

http_request generate_preamble(http_method m)
{
	http_request message{true};
	message.keepalive(true);
	message.protocol(proto_version::HTTP11);
	message.method(m);
	message.schema(schema);
	message.urihost(urihost);
	message.port(port);
	message.path(path);
	message.query(query);
	message.hostname(hostname);
	message.date(date);
	message.header(http::hf_transfer_encoding, tranfertype);
	message.header(customheader, customval0);
	message.header(customheader, customval1);
	message.header(customheader, customval2);
// 	message.cyn_date(cyndate);
	return message;
}

template<class T>
void test_decoding(T& msg, std::string in)
{
	std::string out;
	auto scb = [&msg](http::http_structured_data** data)
	{
		*data = &msg;
	};

	auto hcb = [&msg,&out]()
	{
		out.append(codec.encode_header(msg));
	};

	auto bcb = [&out](std::string&& c)
	{
		EXPECT_FALSE(c.empty());
		out.append(codec.encode_body(c));
	};

	auto tcb = [&out](std::string&& k, std::string&& v)
	{
		EXPECT_FALSE(k.empty());
		EXPECT_FALSE(v.empty());
		out.append(codec.encode_trailer(k,v));
	};

	auto ccb = [&out]()
	{
		out.append(codec.encode_eom());
	};

	auto fcb = [](int,bool&)
	{
		FAIL();
	};

	codec.register_callback(scb,hcb,bcb,tcb,ccb,fcb);
	codec.decode(in.data(), in.size());
	EXPECT_EQ(std::string(in), std::string(out));
}

void test_request_decoding(std::string in)
{
	http_request msg;
	test_decoding(msg, in);
}

void test_response_decoding(std::string in)
{
	http_response msg;
	test_decoding(msg, in);
}

void test_request_encoding(http_request *in_, const std::string& in_data)
{
	http_request &in_msg = *in_;
	http_request out_msg{true};
	std::string out;

	auto scb = [&out_msg](http::http_structured_data** data){*data = &out_msg;};
	auto hcb = [](){};
	auto bcb = [&out](std::string&& c)
	{
		ASSERT_FALSE(c.empty());
		out.append(c);
	};
	auto tcb = [&out_msg](std::string&& k, std::string&& v)
	{
		ASSERT_FALSE(k.empty());
		ASSERT_FALSE(v.empty());
		out_msg.header(k,v);
	};
	auto ccb = [](){};
	auto fcb = [](int,bool&){FAIL();};

	//encode phase.
	std::string encoded_msg;
	encoded_msg.append(codec.encode_header(in_msg));
	encoded_msg.append(codec.encode_body(in_data));
	encoded_msg.append(codec.encode_eom());

	codec.register_callback(scb,hcb,bcb,tcb,ccb,fcb);
	codec.decode(encoded_msg.data(), encoded_msg.size());

	EXPECT_TRUE(out_msg.protocol() == in_msg.protocol());
	EXPECT_TRUE(out_msg.content_len() == in_msg.content_len());
	if(in_msg.date().size())
	{
		EXPECT_TRUE(out_msg.date() == in_msg.date());
	}

	//REQ
	EXPECT_TRUE(out_msg.method() == in_msg.method());
	// note: we are aware that we lose the "schema info" in serialization
	EXPECT_TRUE(out_msg.schema().empty() || out_msg.schema() == in_msg.schema());
	EXPECT_TRUE(out_msg.urihost() == in_msg.urihost());
	EXPECT_TRUE(out_msg.port() == in_msg.port());
	EXPECT_TRUE(out_msg.path() == in_msg.path());
	EXPECT_TRUE(out_msg.query() == in_msg.query());
	EXPECT_EQ(std::string(in_data), out);
}

void test_response_encoding(http_response *in_, const std::string& in_data)
{
	http_response &in_msg = *in_;
	http_response out_msg;

	std::string out;
	auto scb = [&out_msg](http::http_structured_data** data){*data = &out_msg;};
	auto hcb = [](){};
	auto bcb = [&out](std::string&& c)
	{
		ASSERT_TRUE(!c.empty());
		out.append(c);
	};
	auto tcb = [&out_msg](std::string&& k, std::string&& v)
	{
		ASSERT_TRUE(!k.empty());
		ASSERT_TRUE(!v.empty());
		out_msg.header(k,v);
	};
	auto ccb = [](){};
	auto fcb = [](int,bool&){FAIL();};

	//encode phase.
	std::string encoded_msg;
	encoded_msg.append(codec.encode_header(in_msg));
	encoded_msg.append(codec.encode_body(in_data));
	encoded_msg.append(codec.encode_eom());

	codec.register_callback(scb,hcb,bcb,tcb,ccb,fcb);
	codec.decode(encoded_msg.data(), encoded_msg.size());

	EXPECT_TRUE(out_msg.protocol() == in_msg.protocol());
	EXPECT_TRUE(out_msg.content_len() == in_msg.content_len());
	if(in_msg.date().size())
	{
		EXPECT_TRUE(out_msg.date() == in_msg.date());
	}

	EXPECT_TRUE(out_msg.status_code() == in_msg.status_code());
	EXPECT_TRUE(out_msg.status_message() == in_msg.status_message());
	EXPECT_EQ(std::string(in_data), out);
}

void test_encoding(http_structured_data& in_msg, const std::string& in_data)
{
	if(in_msg.type() == typeid(http_request))
		test_request_encoding(static_cast<http_request*>(&in_msg), in_data);
	else
		test_response_encoding(static_cast<http_response*>(&in_msg), in_data);
}

TEST( codec, decode_request )
{
	test_request_decoding(request_generator(HTTP_GET, "").c_str());
	test_request_decoding(request_generator(HTTP_HEAD, "").c_str());
	test_request_decoding(request_generator(HTTP_OPTIONS, "").c_str());
	test_request_decoding(request_generator(HTTP_DELETE, "").c_str());
	test_request_decoding(request_generator(HTTP_PUT, "PiPp0").c_str());
	test_request_decoding(request_generator(HTTP_PATCH, "PiPp0").c_str());
}

TEST( codec, decode_response )
{
	string res = "HTTP/1.1 200 OK\r\n"
		"connection: keep-alive\r\n"
		"content-length: 21\r\n"
		"content-type: text/html; charset=utf-8\r\n"
		"date: Tue, 17 May 2016 14:53:09 GMT\r\n"
		"etag: W/\"2b-y1De87r7kN7VRjYOfNc+SQ\"\r\n"
		"x-cyn-date: 1470328824\r\n"
		"\r\n"
		"ABCDEFGHILMNOPQRSTUVZ";
	test_response_decoding(res.c_str());
}

TEST( codec, decode_chunked_req )
{
	string req = "POST /path HTTP/1.1\r\n"
		"connection: keep-alive\r\n"
		"date: Wed,  6 Jul 2016 11:07:20 CEST\r\n"
		"host: localhost\r\n"
		"test: test_1, test_2\r\n"
		"trailer: Date\r\n"
		"transfer-encoding: Chunked\r\n"
		"x-cyn-date: 1470328824\r\n"
		"\r\n"
		"4\r\n1st \r\n"
		"1e\r\n2nd (last one has to be empty)\r\n"
		"0\r\n"
		"date: Tue, 17 May 2016 14:53:09 GMT\r\n"
		"\r\n";
	test_request_decoding(req.c_str());
}

TEST( codec, decode_chunked_res )
{
	string res = "HTTP/1.1 200 OK\r\n"
		"connection: keep-alive\r\n"
		"trailer: Date\r\n"
		"transfer-encoding: Chunked\r\n"
		"x-cyn-date: 1470328824\r\n"
		"\r\n"
		"4\r\n1st \r\n"
		"1e\r\n2nd (last one has to be empty)\r\n"
		"0\r\n"
		"date: Tue, 17 May 2016 14:53:09 GMT\r\n"
		"\r\n";
	test_response_decoding(res.c_str());
}

TEST( codec, decode_splitted_trailer )
{
	std::string res = "HTTP/1.1 200 OK\r\n"
		"connection: keep-alive\r\n"
		"trailer: Date\r\n"
		"transfer-encoding: Chunked\r\n"
		"x-cyn-date: 1470328824\r\n"
		"\r\n"
		"4\r\n1st \r\n"
		"1e\r\n2nd (last one has to be empty)\r\n"
		"0\r\n";

	std::string t1 = "da";
	std::string t2 = "te: Tue, 17 ";
	std::string t3 = "May 2016 14:53:09 GMT\r\n"
		"\r\n";

	http_response msg;
	bool eom{false};

	auto scb = [&msg](http::http_structured_data** data){*data=&msg;};
	auto hcb = [](){};
	auto bcb = [](std::string&&){};
	auto tcb = [](std::string&& k , std::string&& v)
	{
		ASSERT_EQ(std::string(k),"date");
		ASSERT_EQ(std::string(v),"Tue, 17 May 2016 14:53:09 GMT");
	};

	auto ccb = [&eom]()
	{
		eom=true;
	};

	auto fcb = [](int,bool&)
	{
		FAIL();
	};

	codec.register_callback(scb,hcb,bcb,tcb,ccb,fcb);
	codec.decode( res.data(), res.size() );
	codec.decode( t1.data(), t1.size() );
	codec.decode( t2.data(), t2.size() );
	codec.decode( t3.data(), t3.size() );
}

TEST( codec, encode_req_header )
{
	auto message = generate_preamble(HTTP_HEAD);
	test_encoding(message,std::string{});
}

TEST( codec, encode_res_header )
{
	std::string chunks;
	http_response message;
	message.protocol(proto_version::HTTP11);
	message.status(200);
	message.date(date);
	test_encoding(message,chunks);
}

TEST(codec, encode_full_req)
{
	auto message = generate_preamble(HTTP_PUT);
	std::string body{"Nel mezzo del cammin di nostra vita mi trovai per un pacchetto oscuro, che l'HTTP s'era smarrito."};
	message.content_len(body.size());
	test_encoding(message, body);
}

TEST( codec, encode_full_res )
{
	http_response message;
	message.protocol(proto_version::HTTP11);
	message.status(200);
	message.date(date);
	std::string body{"Nel mezzo del cammin di nostra vita mi trovai per un pacchetto oscuro, che l'HTTP s'era smarrito."};
	message.content_len(body.size());
	test_encoding(message,body);
}

TEST(codec, encode_chunked_req)
{
	std::string chunks;
	auto message = generate_preamble(HTTP_POST);
	message.chunked(true);
	string c0(16,'a');
	chunks.append(c0);
	string c1(8,'b');
	chunks.append(c1);
	test_encoding(message,chunks);
}

TEST( codec, encode_chunked_res )
{
	std::string chunks;
	http_response message;
	message.protocol(proto_version::HTTP11);
	message.status(200);
	message.date(date);
	message.chunked(true);
	string c0(16,'a');
	chunks.append(c0);
	string c1(8,'b');
	chunks.append(c1);
	test_encoding(message,chunks);
}

TEST( codec, keepalive )
{
	http::http_request msg{true};
	bool eom{false};

	auto scb = [&msg](http::http_structured_data** data){*data=&msg;};
	auto hcb = [](){};
	auto bcb = [](std::string&&){};
	auto tcb = [](std::string&&, std::string&&){};
	auto ccb = [&eom](){eom=true;};
	auto fcb = [](int,bool&){FAIL();};
	codec.register_callback(scb,hcb,bcb,tcb,ccb,fcb);

	std::string msg_ = "GET /path HTTP/1.0\r\nconnection: keep-alive\r\n\r\n";
	codec.decode(msg_.data(), msg_.size());
	ASSERT_TRUE(eom);
	EXPECT_TRUE(msg.keepalive());
	eom=false;

	msg_ = "GET /path HTTP/1.0\r\nconnection: close\r\n\r\n";
	codec.decode(msg_.data(), msg_.size());

	ASSERT_TRUE(eom);
	EXPECT_FALSE(msg.keepalive());
	eom=false;

	msg_ = "GET /path HTTP/1.1\r\n\r\n";
	codec.decode(msg_.data(), msg_.size());
	ASSERT_TRUE(eom);
	EXPECT_TRUE(msg.keepalive());
	eom=false;

	msg_ = "GET /path HTTP/1.0\r\n\r\n";
	codec.decode(msg_.data(), msg_.size());
	ASSERT_TRUE(eom);
	EXPECT_FALSE(msg.keepalive());
}

/*
 * Test the behavior of the _decoder in the case
 * of Transfer-Encoding: Chunked with empty body
 */
TEST( codec, chunked_with_empty_body ) // see DRM-280
{
	const std::string raw_response = "HTTP/1.1 201 Created\r\n"
									 "connection: keep-alive\r\n"
									 "date: Thu, 08 Sep 2016 09:40:54 GMT\r\n"
									 "server: Door-mat\r\n"
									 "transfer-encoding: Chunked\r\n"
									 "via: HTTP/1.1\r\n"
									 "x-powered-by: express\r\n\r\n0\r\n\r\n";

	http_codec encoder;
	http_response response;
	std::string out;
	auto codec_scb = [&response](http::http_structured_data** data)
	{
		*data = &response;
	};

	auto codec_hcb = [&encoder, &response, &out]()
	{
		out.append(encoder.encode_header(response));
	};

	auto codec_bcb = [&encoder](std::string&&)
	{

	};

	auto codec_tcb = [&encoder](std::string&&, std::string&&)
	{

	};

	auto codec_ccb = [&encoder, &out]()
	{
		out.append(encoder.encode_eom());
	};

	auto codec_fcb = [&encoder](int, bool&) {};

	http_codec decoder;
	decoder.register_callback(codec_scb,codec_hcb,codec_bcb,codec_tcb,codec_ccb,codec_fcb);

	decoder.decode(raw_response.data(), raw_response.size());
	EXPECT_EQ(std::string(out),raw_response);
}

TEST( codec, ignore_unexpected_content_len )
{
	std::string req{"GET /"};
	req += " HTTP/1.1\r\n";
	req += "content-length: 0\r\n";
	req += "content-length: 21\r\n";
	req += "\r\n";

	http::http_codec::structured_cb scb;
	http::http_codec::void_cb hcb;
	http::http_codec::stream_cb bcb;
	http::http_codec::trailer_cb tcb;
	http::http_codec::void_cb ccb;

	uint8_t cb_count{0};

	http::http_request r;
	auto codec_scb =[&cb_count, &r](http::http_structured_data** data)
	{
		*data = &r;
		++cb_count;
	};

	auto codec_hcb = [&cb_count]()
	{
		++cb_count;
	};

	auto codec_bcb = [](std::string&&)
	{
		FAIL();
	};

	auto codec_tcb = [](std::string&&, std::string&&)
	{
		FAIL();
	};

	auto codec_ccb = [&cb_count]()
	{
		++cb_count;
	};

	auto codec_fcb = [&cb_count](int code, bool& fatal)
	{
		++cb_count;
		EXPECT_EQ(code, HPE_UNEXPECTED_CONTENT_LENGTH);
		fatal = false;
	};

	http_codec decoder;
	decoder.register_callback(codec_scb,codec_hcb,codec_bcb,codec_tcb,codec_ccb,codec_fcb);
	decoder.decode(req.data(), req.size());
	EXPECT_EQ(cb_count,4);
}

TEST( codec, too_long_uri )
{
	std::string too_long_uri(65535,'K');
	std::string req{"GET /"};
	req += too_long_uri;
	req += " HTTP/1.1\r\n";
	req += "\r\n";

	http::http_codec::structured_cb scb;
	http::http_codec::void_cb hcb;
	http::http_codec::stream_cb bcb;
	http::http_codec::trailer_cb tcb;
	http::http_codec::void_cb ccb;

	http::http_request r;
	auto codec_scb =[&r](http::http_structured_data** data)
	{
		*data = &r;
	};

	auto codec_hcb = []()
	{
		FAIL();
	};

	auto codec_bcb = [](std::string&&)
	{
		FAIL();
	};

	auto codec_tcb = [](std::string&&, std::string&&)
	{
		FAIL();
	};

	auto codec_ccb = []()
	{
		FAIL();
	};

	bool fcb_called{false};
	auto codec_fcb = [&fcb_called](int code, bool& fatal)
	{
		fcb_called = true;
		EXPECT_EQ(code, HPE_CB_url);
		EXPECT_TRUE(fatal);
	};

	http_codec decoder;
	decoder.register_callback(codec_scb,codec_hcb,codec_bcb,codec_tcb,codec_ccb,codec_fcb);
	decoder.decode(req.data(), req.size());

	EXPECT_TRUE(fcb_called);
}

}//namespace

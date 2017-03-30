#include <gtest/gtest.h>
#include <mock_server/mock_server.h>
#include "../src/log/inspector_serializer.h"
#include "../src/service_locator/service_initializer.h"
#include "../src/utils/log_wrapper.h"
#include "../src/endpoints/path/radix_tree.h"
#include "nodes/common.h"

using namespace logging;

struct fake_writer : public writer
{
	std::string name;
	std::string output;
	void open( const std::string &s ) override
	{
//		LOGTRACE("File : ", s );
		name = s;
	}

	writer& operator<<( const std::string &line ) override
	{
//		LOGTRACE("Line :", line );
		output.append( line ).append("\n");
		return *this;
	}
};

static fake_writer* w;

class ilog_mock_conf: public test_utils::preset::mock_conf
{
public:
	std::string get_log_level() const noexcept override
	{
		return "debug";
	}
};

struct inspector_log_test : public ::testing::Test
{
protected:
	virtual void SetUp() override
	{
		w = new fake_writer; // inspector_log get rid of this
		test_utils::preset::setup(new ilog_mock_conf{}, new inspector_log{ "prefix", "filename", "trace", w } );
	}

	virtual void TearDown() override
	{
		test_utils::preset::teardown();
	}
};

TEST_F(inspector_log_test, creation)
{
	ASSERT_EQ( "prefix/filename.json", w->name );
}

TEST_F(inspector_log_test, usage)
{
	access_recorder ar;

	dstring body("yeeee this is long");
	http::http_request req;
	req.protocol(http::proto_version::HTTP11);
	req.urihost("cynny.com");
	req.path("/pippo");
	req.method("GET");
	req.hostname("hostname.com");
	req.header("connection", "yes");
	req.header("keep-alive", "no");
	req.header("public", "perhaps");
	req.header("proxy-authenticate", "");
	req.header("transfer-encoding", "chunkEd");
	req.header("upgrade", "None");
	req.header("goodenough...", "Fuck");
	req.header("referrer", "http://www.google.it");
	req.header("content-length", dstring::to_string( body.size() ) );
	req.header("host", "video.cynny.org");
	ar.request( req );
	ar.append_request_body( body );
	ar.append_request_trailer("trailer", "Yes");

	http::http_response resp;
	dstring rbody{"Short stuff"};
	resp.status(200);
	resp.header("connection", "yes");
	resp.header("content-length", dstring::to_string( rbody.size() ) );
	resp.header("public", "Fuck");

	ar.response( resp );
	ar.append_response_body( rbody );
	ar.append_response_trailer("trailer", "response");

	service::locator::inspector_log().log( std::move( ar ) );

	ASSERT_EQ( "{\n    \"request\": {\n        \"body\": \"eWVlZWUgdGhpcyBpcyBsb25n\",\n        \"fragment\": \"\",\n        \"headers\": {\n            \"connection\": \"yes\",\n            \"content-length\": \"18\",\n            \"goodenough...\": \"Fuck\",\n            \"host\": [\n                \"hostname.com\",\n                \"video.cynny.org\"\n            ],\n            \"keep-alive\": \"no\",\n            \"proxy-authenticate\": \"\",\n            \"public\": \"perhaps\",\n            \"referrer\": \"http://www.google.it\",\n            \"transfer-encoding\": \"chunkEd\",\n            \"upgrade\": \"None\"\n        },\n        \"method\": \"GET\",\n        \"port\": \"\",\n        \"protocol\": \"\",\n        \"query\": \"\",\n        \"schema\": \"\",\n        \"trailers\": {\n            \"trailer\": \"Yes\"\n        },\n        \"urihost\": \"cynny.com\"\n    },\n    \"response\": {\n        \"body\": \"U2hvcnQgc3R1ZmY=\",\n        \"headers\": {\n            \"connection\": \"yes\",\n            \"content-length\": \"11\",\n            \"public\": \"Fuck\"\n        },\n        \"status\": 200,\n        \"trailers\": {\n            \"trailer\": \"response\"\n        }\n    }\n}\n", w->output );
}

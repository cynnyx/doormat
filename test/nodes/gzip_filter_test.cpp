#include "../../src/requests_manager/gzip_filter.h"
#include "common.h"

#include <zlib.h>

using namespace test_utils;

int gzip(const char* in, size_t ilen, char* out, size_t ulen)
{
	z_stream stream;
	stream.next_in = (Bytef*)in;
	stream.avail_in = ilen;
	stream.next_out = (Bytef*)out;
	stream.avail_out = ulen;
	stream.zalloc = Z_NULL;
	stream.zfree = Z_NULL;
	stream.opaque = Z_NULL;

	deflateInit2(&stream, 9, Z_DEFLATED, MAX_WBITS|16, 8, Z_DEFAULT_STRATEGY);
	EXPECT_EQ(deflate(&stream, Z_FINISH), Z_STREAM_END);
	deflateEnd(&stream);

	return stream.total_out;
}

int extract(const char* in, size_t ilen, char* out, size_t ulen)
{
	z_stream stream;
	stream.next_in = (Bytef*)in;
	stream.avail_in = ilen;
	stream.next_out = (Bytef*)out;
	stream.avail_out = ulen;
	stream.zalloc = Z_NULL;
	stream.zfree = Z_NULL;
	stream.opaque = Z_NULL;

	inflateInit2(&stream, MAX_WBITS|16);
	EXPECT_EQ(inflate(&stream, Z_FINISH), Z_STREAM_END);
	inflateEnd(&stream);

	return stream.total_out;
}


struct gzip_mock_conf : public preset::mock_conf
{
	static bool enabled;
	static bool mime_match;
	static uint8_t level;
	static uint32_t minsize;
	bool get_compression_enabled() const noexcept override { return enabled; }
	bool comp_mime_matcher(const std::string&) const noexcept override { return mime_match; }
	uint8_t get_compression_level() const noexcept override { return level; }
	uint32_t get_compression_minsize() const noexcept override { return minsize; }
};
bool gzip_mock_conf::enabled{false};
bool gzip_mock_conf::mime_match{false};
uint8_t gzip_mock_conf::level{9};
uint32_t gzip_mock_conf::minsize{0};

struct gzipfilter_test : public ::testing::Test
{
	std::unique_ptr<node_interface> ch;
protected:
	virtual void SetUp() override
	{
		preset::setup(new gzip_mock_conf{});
		preset::init_thread_local();
		last_node::reset();
	}
};

TEST_F(gzipfilter_test, disabled)
{
	http::http_request req;
	req.method("GET");
	req.header("accept-encoding","gzip");
	req.header("content-type", "application/json");

	std::string body{"BIBIDIbobidyBu"};
	last_node::body_parts.clear();
	last_node::body_parts.push_back(body.c_str());

	ch = make_unique_chain<node_interface, first_node, nodes::gzip_filter, last_node>();
	ch->on_request_preamble(std::move(req));
	ch->on_request_finished();

	ASSERT_TRUE(first_node::response.is_initialized());
	EXPECT_FALSE(first_node::response->has("content-encoding", "gzip"));
	EXPECT_FALSE(first_node::err);
	EXPECT_GE(first_node::res_body, 1U);
	EXPECT_EQ(std::string(first_node::res_body_str), body);
	EXPECT_EQ(first_node::res_trailer, 1U);
	EXPECT_EQ(first_node::res_eom, 1U);
}

TEST_F(gzipfilter_test, below_min)
{
	gzip_mock_conf::enabled = true;
	gzip_mock_conf::minsize = 1024;

	http::http_request req;
	req.method("GET");
	req.header("accept-encoding","gzip");
	req.header("content-type", "application/json");

	std::string body{"BIBIDIbobidyBu"};
	last_node::body_parts.clear();
	last_node::body_parts.push_back(body.c_str());

	ch = make_unique_chain<node_interface, first_node, nodes::gzip_filter, last_node>();
	ch->on_request_preamble(std::move(req));
	ch->on_request_finished();

	ASSERT_TRUE(first_node::response.is_initialized());
	EXPECT_FALSE(first_node::response->has("content-encoding", "gzip"));
	EXPECT_FALSE(first_node::err);
	EXPECT_GE(first_node::res_body, 1U);
	EXPECT_EQ(std::string(first_node::res_body_str), body);
	EXPECT_EQ(first_node::res_trailer, 1U);
	EXPECT_EQ(first_node::res_eom, 1U);
}

TEST_F(gzipfilter_test, mime_not_match)
{
	gzip_mock_conf::enabled = true;
	gzip_mock_conf::mime_match = false;
	gzip_mock_conf::minsize = 0;

	http::http_request req;
	req.method("GET");
	req.header("accept-encoding","gzip");
	req.header("content-type", "application/json");

	std::string body{"BIBIDIbobidyBu"};
	last_node::body_parts.clear();
	last_node::body_parts.push_back(body.c_str());

	ch = make_unique_chain<node_interface, first_node, nodes::gzip_filter, last_node>();
	ch->on_request_preamble(std::move(req));
	ch->on_request_finished();

	ASSERT_TRUE(first_node::response.is_initialized());
	EXPECT_FALSE(first_node::response->has("content-encoding", "gzip"));
	EXPECT_FALSE(first_node::err);
	EXPECT_GE(first_node::res_body, 1U);
	EXPECT_EQ(std::string(first_node::res_body_str), body);
	EXPECT_EQ(first_node::res_trailer, 1U);
	EXPECT_EQ(first_node::res_eom, 1U);
}

TEST_F(gzipfilter_test, normal_transfer)
{
	gzip_mock_conf::enabled = true;
	gzip_mock_conf::mime_match = true;
	gzip_mock_conf::minsize = 0;

	http::http_request req;
	req.method("GET");
	req.header("accept-encoding","gzip");
	req.header("content-type", "application/json");

	std::string body(512, 'a');
	http::http_response res;
	res.protocol(http::proto_version::HTTP11);
	res.status(200);
	res.content_len(body.size());

	last_node::response.reset(res);
	last_node::body_parts.clear();
	last_node::body_parts.push_back(body.c_str());

	ch = make_unique_chain<node_interface, first_node, nodes::gzip_filter, last_node>();
	ch->on_request_preamble(std::move(req));
	ch->on_request_finished();

	ASSERT_TRUE(first_node::response.is_initialized());
	EXPECT_TRUE(first_node::response->has(http::hf_content_encoding, http::hv_gzip));
	EXPECT_EQ(first_node::res_body, 1U);
	EXPECT_EQ(first_node::res_eom, 1U);
	EXPECT_FALSE(first_node::err);

	auto len = first_node::res_body_str.size();
	uint32_t crc = *(uint32_t*)first_node::res_body_str.substr( len - 8 ).data();
	uint32_t osize = *(uint32_t*)first_node::res_body_str.substr( len - 4 ).data();
	EXPECT_EQ( crc , crc32(0L, (Bytef*)body.data(), body.size()) );
	EXPECT_EQ( osize , body.size() );

	char buf[1024];
	len = extract( first_node::res_body_str.data(), first_node::res_body_str.size(), buf, 1024);
	EXPECT_EQ( body, std::string(buf, len));
}

TEST_F(gzipfilter_test, chunked_transfer)
{
	gzip_mock_conf::enabled = true;
	gzip_mock_conf::mime_match = true;
	gzip_mock_conf::minsize = 0;

	http::http_request req;
	req.method("GET");
	req.header("accept-encoding","gzip");
	req.header("content-type", "application/json");

	last_node::body_parts.clear();

	std::string body(512, 'a');
	last_node::body_parts.push_back(body.c_str());
	last_node::body_parts.push_back(body.c_str());
	last_node::body_parts.push_back(body.c_str());
	last_node::body_parts.push_back(body.c_str());

	ch = make_unique_chain<node_interface, first_node, nodes::gzip_filter, last_node>();
	ch->on_request_preamble(std::move(req));
	ch->on_request_finished();

	ASSERT_TRUE(first_node::response.is_initialized());
	EXPECT_TRUE(first_node::response->has("content-encoding", "gzip"));
	EXPECT_FALSE(first_node::err);
	EXPECT_GE(first_node::res_body, 1U);
	EXPECT_EQ(first_node::res_trailer, 1U);
	EXPECT_EQ(first_node::res_eom, 1U);

	char buf[2048];
	auto len = extract( first_node::res_body_str.data(), first_node::res_body_str.size(), buf, sizeof(buf));
	EXPECT_EQ(body+body+body+body, std::string(buf, len));
}

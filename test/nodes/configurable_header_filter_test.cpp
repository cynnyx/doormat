#include <gtest/gtest.h>

#include <list>

#include "../src/configuration/header_configuration.h"
#include "../src/requests_manager/configurable_header_filter.h"
#include "common.h"

namespace
{
using namespace test_utils;

configuration::header h;
	
struct local_conf : public preset::mock_conf
{
	class fake_stream
	{
		std::vector<std::string> fileTokens
		{
			"proxy_set_header", "cyn-puppa", "on",
			"proxy_set_header", "cyn-lippa", "off",
			"proxy_set_header", "x-cyn-date", "$timestamp",
			"proxy_set_header", "r-cyn-time", "$msec",
			"proxy_set_header", "x-scheme", "$scheme",
			"proxy_set_header", "x-origin", "$remote_addr", 
			"proxy_set_header", "pippo", "$kill"
		};
		mutable std::size_t index{0};
		bool go{true};
		bool kill{false};
	public:
		fake_stream() = default;
		bool is_open() const noexcept { return true; }
		bool eof() const noexcept { return go == false || index == fileTokens.size(); }
		
		const fake_stream& operator>> ( std::string& out ) const
		{
			std::string token = fileTokens[index++];
			if ( ! kill && token == "$kill" )
				token = "topolino";
			out = token;
			return *this;
		}
		void set_go( bool g ) noexcept { go = g; }
		void set_kill( bool k ) noexcept { kill = k; }
	};
	bool go{true};
	bool kill{false};
	local_conf( bool g = true, bool k = false ): go(g), kill(k) {}

	const configuration::header& header_configuration() const noexcept override
	{
		if ( go )
		{
			fake_stream fs;
			fs.set_go( go );
			fs.set_kill( kill );
			h.init<fake_stream>( fs ); 
		}
		return h;
	}
};

std::unique_ptr<node_interface> ch;
struct conf_header_filter : public ::testing::Test
{
protected:
	virtual void SetUp() override
	{
	}

	virtual void TearDown() override
	{
		preset::teardown(ch);
	}
};

struct conf_header_filter2 : public ::testing::Test
{
protected:
	virtual void SetUp() override
	{
	}

	virtual void TearDown() override
	{
		preset::teardown(ch);
// 		last_node::reset();
	}
};
}

void make_request( bool cyndate = true )
{
	auto addr = boost::asio::ip::address::from_string("127.0.0.1");
	http::http_request req{true};
	req.origin( addr );
	req.method("GET");
	req.hostname("hostname");
	req.header(http::hf_connection, "close");
	if(cyndate)
		req.header(http::hf_cyndate, "1");
	req.header("pippo","666");

	ch = make_unique_chain<node_interface, first_node, nodes::configurable_header_filter, last_node>();
	ch->on_request_preamble(std::move(req));
	ch->on_request_body({});
	ch->on_request_trailer({},{});
	ch->on_request_finished();
}

TEST_F(conf_header_filter, empty)
{
	preset::setup(new local_conf{ false });
	make_request();
	
	bool ok = last_node::request != boost::none;
	ASSERT_TRUE( ok );

	http::http_request result = last_node::request.get();
	ASSERT_TRUE( result.header( http::hf_cyndate ) == "1" );
	ASSERT_TRUE( result.header("pippo")  == "666" );
}

TEST_F(conf_header_filter, full)
{
	preset::setup(new local_conf{});
	make_request( false );
	
	bool ok = last_node::request != boost::none;
	ASSERT_TRUE( ok );
	
	http::http_request result = last_node::request.get();
	auto xs = result.header("x-scheme");
	ASSERT_TRUE( result.header("pippo")  == "666" );
	ASSERT_EQ( result.header(http::hf_cyndate).size(), 10U );
	ASSERT_EQ( result.header("r-cyn-time").size(), 13U );
	ASSERT_TRUE( utils::icompare(xs, "https") );
	//FIXME
	/*
	auto& dheaders = result.destination_headers_list();
	ASSERT_TRUE( dheaders.size() == 1 ) << dheaders.size();
	ASSERT_TRUE( dheaders.front() == "x-origin" ) << std::string(dheaders.front());
	*/
}

TEST_F(conf_header_filter, kill)
{
	ASSERT_TRUE( last_node::request == boost::none );
	preset::setup(new local_conf{true, true});
	make_request( true );
	ASSERT_TRUE( last_node::request != boost::none );
	
	http::http_request result = last_node::request.get();
	auto hl = result.headers( "pippo" );
	ASSERT_TRUE( hl.empty() );
}

#include <gtest/gtest.h>

#include <utility>

#include "../src/http/http_commons.h"
#include "../src/chain_of_responsibility/chain_of_responsibility.h"
#include "../src/requests_manager/id_header_adder.h"
#include "../src/constants.h"

http::http_response resp;

struct id_header_adder_test : public ::testing::Test
{
	static http::http_request request;
	static http::http_response response;
	struct last_node : public node_interface
	{
		using node_interface::node_interface;

		last_node() {}

		void on_request_preamble(http::http_request&& r)
		{
			request = std::move ( r );
			on_header( std::move ( resp ) );
		}
	};
	struct first_node : public node_interface
	{
		using node_interface::node_interface;

		first_node() {}

		void on_header(http::http_response&& r)
		{
			response = std::move(r);
		}
	};
	std::unique_ptr<node_interface> ch;
protected:
	void SetUp() override
	{
		http::http_request t;
		request = std::move( t );
		http::http_response r;
		response = std::move( r );
		http::http_response re;
		resp = std::move( re );

		ch = make_unique_chain<node_interface, first_node, nodes::id_header_adder, last_node>();
	}
};

http::http_request id_header_adder_test::request;
http::http_response id_header_adder_test::response;

static const std::string hostname { "burp.it" };
static const std::string via_h { "HTTP/1.1 pippo.org" };
static const std::string expected{ via_h + ", HTTP/2.0 burp.it" };
static const std::string servername = project_name() + "-" + version();

TEST_F(id_header_adder_test, request)
{
	http::http_request req(true);
	req.header( http::hf_via, via_h.c_str() );
	req.hostname( hostname.c_str() );
	req.protocol( http::proto_version::HTTP20 );
	ch->on_request_preamble( std::move( req ) );

	const auto& server = request.header(http::hf_server);
	ASSERT_TRUE( server );
	EXPECT_EQ( servername, std::string(server) );
}

TEST_F(id_header_adder_test, response)
{
	resp.header( http::hf_via, via_h.c_str() );
	resp.hostname( hostname.c_str() );
	resp.protocol( http::proto_version::HTTP20 );

	ch->on_request_preamble ( std::move( http::http_request{} ) );

	const auto& via_header = response.header(http::hf_via);
	ASSERT_TRUE( via_header );
	EXPECT_EQ( expected, std::string(via_header) );

	const auto& server = response.header( http::hf_server );
	ASSERT_TRUE( server );
	EXPECT_EQ( servername, std::string(server) );
}

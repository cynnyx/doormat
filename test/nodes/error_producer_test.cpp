#include "../src/requests_manager/error_producer.h"
#include "common.h"

namespace
{
using namespace test_utils;
struct error_producer_test : public preset::test {};
}

TEST_F(error_producer_test, usage)
{
	auto ec = INTERNAL_ERROR(static_cast<uint16_t>(errors::http_error_code::bad_gateway));
	auto init_fn = [this, &ec](boost::asio::io_service& ios)
	{
		preset::init_thread_local();
		ch = make_unique_chain<node_interface, first_node, nodes::error_producer, last_node>();
		ch->on_request_preamble({});
		ch->on_request_body({});
		ch->on_request_trailer({}, {});
		ch->on_request_canceled(ec);
		//The following should not land anywhere
		ch->on_request_preamble({});
		ch->on_request_body({});
		ch->on_request_trailer({}, {});
		ch->on_request_finished();
	};

	service::locator::service_pool().run(init_fn);

	EXPECT_TRUE(last_node::request.is_initialized());
	EXPECT_EQ(last_node::req_body, 1U);
	EXPECT_EQ(last_node::req_trailer, 1U);
	EXPECT_TRUE( last_node::err == ec ) << last_node::err;

	ASSERT_TRUE(first_node::response.is_initialized());
	EXPECT_EQ(first_node::response->status_code(),(uint16_t)errors::http_error_code::bad_gateway);
	EXPECT_GE(first_node::res_body, 1U);
	EXPECT_EQ(first_node::res_eom, 1U);
}

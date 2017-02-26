#include <gtest/gtest.h>

#include "../src/requests_manager/date_setter.h"
#include "../src/chain_of_responsibility/chain_of_responsibility.h"

namespace
{

struct date_setter_test : public ::testing::Test
{
	struct last_node : node_interface
	{
		using node_interface::node_interface;
		last_node() {}

		void on_request_preamble(http::http_request&& r) { ++preamble_called; }
		void on_request_body(dstring&& c) { ++body_called; }
		void on_request_trailer(dstring&& k, dstring&& v) { ++trailer_called; }
		void on_request_finished()
		{
			++finished_called;
			http::http_response res;
			ASSERT_FALSE(res.date().size());
			on_header(std::move(res));
			on_body({});
			on_trailer({}, {});
			on_end_of_message();
		}

		static uint preamble_called;
		static uint body_called;
		static uint trailer_called;
		static uint finished_called;
	};

	struct first_node : node_interface
	{
		using node_interface::node_interface;

		first_node() {}

		void on_header(http::http_response&& r)
		{
			response = std::move(r);
		}

		void on_body(dstring&& chunk)
		{
			++body_called;
		}

		void on_trailer(dstring&&, dstring&&)
		{
			++trailer_called;
		}

		void on_end_of_message()
		{
			++eom_called;
			base::on_end_of_message();
		}

		static http::http_response response;
		static uint body_called;
		static uint trailer_called;
		static uint eom_called;
		static uint ack_called;
	};

public:
	std::unique_ptr<node_interface> ch;

protected:
	void SetUp() override
	{
		last_node::preamble_called = 0;
		last_node::body_called = 0;
		last_node::trailer_called = 0;
		last_node::finished_called = 0;
		ch = make_unique_chain<node_interface, first_node, nodes::date_setter, last_node>();
	}
};

uint date_setter_test::last_node::preamble_called;
uint date_setter_test::last_node::body_called;
uint date_setter_test::last_node::trailer_called;
uint date_setter_test::last_node::finished_called;
http::http_response date_setter_test::first_node::response;
uint date_setter_test::first_node::body_called;
uint date_setter_test::first_node::trailer_called;
uint date_setter_test::first_node::eom_called;
}


TEST_F(date_setter_test, usage)
{
	http::http_request preamble;
	ch->on_request_preamble(std::move(preamble));
	ch->on_request_body({});
	ch->on_request_trailer({},{});
	ch->on_request_finished();

	// check not-filtering
	EXPECT_EQ(date_setter_test::last_node::preamble_called, 1);
	EXPECT_EQ(date_setter_test::last_node::body_called, 1);
	EXPECT_EQ(date_setter_test::last_node::trailer_called, 1);
	EXPECT_EQ(date_setter_test::last_node::finished_called, 1);
	EXPECT_EQ(date_setter_test::first_node::body_called, 1);
	EXPECT_EQ(date_setter_test::first_node::trailer_called, 1);
	EXPECT_EQ(date_setter_test::first_node::eom_called, 1);

	// check date was set
	EXPECT_TRUE(first_node::response.date().size());
}

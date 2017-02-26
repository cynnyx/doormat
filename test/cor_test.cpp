#include <gtest/gtest.h>
#include <boost/asio.hpp>
#include "../src/chain_of_responsibility/chain_of_responsibility.h"
#include "../src/chain_of_responsibility/node_interface.h"
#include "../src/http/http_structured_data.h"
#include "../src/errors/error_codes.h"

namespace
{

int count{0};
boost::asio::io_service local_io_service;

TEST(cor_propagation, request_propagation)
{
	count = 0;
	struct n1 : public node_interface
	{
		using node_interface::node_interface;

        void on_request_preamble(http::http_request&& message) {  ++count; base::on_request_preamble(std::move(message)); }
		void on_request_body(dstring&& chunk) {  ++count; base::on_request_body(std::move(chunk)); }
	};

	struct n2 : public node_interface
	{
		using node_interface::node_interface;

        void on_request_preamble(http::http_request&& message) { ++count; base::on_request_preamble(std::move(message)); }
		void on_request_body(dstring&& chunk) { ++count; base::on_request_body(std::move(chunk)); }
	};

	auto c = make_unique_chain<node_interface,n2, n1, n2, n1,n2, n1, n2, n1>();
    c->initialize_callbacks([](http::http_response &&) {}, [](dstring &&) {}, [](dstring &&, dstring &&) {}, []() {},
                            [](const errors::error_code &) {}, []() {});

	http::http_request msg{true};
	c->on_request_preamble(std::move(msg));
	c->on_request_body({});
	c->on_request_finished();
	ASSERT_EQ(count, 16);
}

TEST(cor_propagation, request_propagation_and_on_body)
{
	count = 0;
	struct n1 : public node_interface
	{
		using node_interface::node_interface;

		n1() {}

		void on_body(dstring&& chunk)
		{
			base::on_body(std::move(chunk));
		}
	};

	struct n2 : public node_interface
	{
		using node_interface::node_interface;

        n2() {}

        void on_body(dstring&& chunk)
		{
			++count;
			base::on_body(std::move(chunk));
		}
	};

	struct n3 : public node_interface
	{
		using node_interface::node_interface;

		n3() {}

		void on_request_preamble(http::http_request&& message)
		{

			base::on_body({});
		}
	};

	auto c = make_unique_chain<node_interface,n1, n2, n3>();
    c->initialize_callbacks([](http::http_response &&) {}, [](dstring &&) { ++count; }, [](dstring &&, dstring &&) {},
                            []() {},
                            [](const errors::error_code &ec) {}, []() {});

	http::http_request msg{true};
	c->on_request_preamble(std::move(msg));
	ASSERT_EQ(count, 2);
}

TEST(cor_propagation, multiple_body_propagation)
{
	count = 0;
	struct n1 : public node_interface
	{
		using node_interface::node_interface;

        n1() {}
    };

	struct n2 : public node_interface
	{
		using node_interface::node_interface;

        n2() {}
    };

	struct n3 : public node_interface
	{
		using node_interface::node_interface;

        n3() {}

        void on_request_preamble(http::http_request&& message)
		{
			base::on_body({});
			base::on_body({});
		}
	};

	auto c = make_unique_chain<node_interface,n1, n2, n3>();
    c->initialize_callbacks([](http::http_response &&) {}, [](dstring &&) { ++count; }, [](dstring &&, dstring &&) {},
                            []() {},
                            [](const errors::error_code &ec) {}, []() {});

	http::http_request msg{true};
	c->on_request_preamble(std::move(msg));
	ASSERT_EQ(count, 2);
}

TEST(cor_propagation, request_propagation_on_header)
{
	count = 0;
	struct n1 : public node_interface
	{
		using node_interface::node_interface;

        n1() {}

        void on_body(dstring&& chunk)
		{
			base::on_body(std::move(chunk));
		}
	};

	struct n2 : public node_interface
	{
		using node_interface::node_interface;

        n2() {}

        void on_header(http::http_response &&h)
		{
			++count;
			base::on_header(std::move(h));
		}
	};

	struct n3 : public node_interface
	{
		using node_interface::node_interface;

        n3() {}

        void on_request_preamble(http::http_request&& message)
		{
			base::on_header({});
		}
	};

	auto c = make_unique_chain<node_interface,n1, n2, n3>();
    c->initialize_callbacks([](http::http_response &&) { ++count; }, [](dstring &&) {}, [](dstring &&, dstring &&) {},
                            []() {},
                            [](const errors::error_code &ec) {}, []() {});

	http::http_request msg{true};
	c->on_request_preamble(std::move(msg));
	ASSERT_EQ(count, 2);
}

TEST(cor_propagation, end_of_message)
{
	count = 0;
	struct n1 : public node_interface
	{
		using node_interface::node_interface;

        n1() {}
    };

	struct n2 : public node_interface
	{
		using node_interface::node_interface;

        n2() {}
    };

	struct n3 : public node_interface
	{
		using node_interface::node_interface;

        n3() {}

        void on_request_preamble(http::http_request&& message)
		{
			base::on_body({});
			base::on_body({});
			base::on_end_of_message();
		}
	};

	auto c = make_unique_chain<node_interface,n1, n2, n3>();
    c->initialize_callbacks([](http::http_response &&) {}, [](dstring &&) { ++count; }, [](dstring &&, dstring &&) {},
                            []() { ++count; },
                            [](const errors::error_code &ec) {}, []() {});

	http::http_request msg{true};
	c->on_request_preamble(std::move(msg));
	ASSERT_EQ(count, 3);
}

TEST(cor_propagation, error)
{
	count = 0;
	struct n1 : public node_interface
	{
		using node_interface::node_interface;

        n1() {}
    };

	struct n2 : public node_interface
	{
		using node_interface::node_interface;

        n2() {}
    };

	struct n3 : public node_interface
	{
		using node_interface::node_interface;

        n3() {}

        void on_request_preamble(http::http_request&& message)
		{
			base::on_error( INTERNAL_ERROR_LONG(500) );
		}
	};

	auto c = make_unique_chain<node_interface,n1, n2, n3>();
    c->initialize_callbacks([](http::http_response &&) { count = 0; }, [](dstring &&) { count = 0; },
                            [](dstring &&, dstring &&) {}, []() { count = 0; },
                            [](const errors::error_code &ec) { ++count; }, []() {});

	http::http_request msg{true};
	c->on_request_preamble(std::move(msg));
	ASSERT_EQ(count, 1);

}

TEST(cor_propagation, trailer_propagation)
{
	count = 0;
	struct n1 : public node_interface
	{
		using node_interface::node_interface;

        n1() {}

		void on_trailer(dstring&& k, dstring&& v)
		{
			base::on_trailer(std::move(k), std::move(v));
		}
	};

	struct n2 : public node_interface
	{
		using node_interface::node_interface;

        n2() {}

		void on_trailer(dstring&& k, dstring&& v)
		{
			++count;
			base::on_trailer(std::move(k), std::move(v));
		}
	};

	struct n3 : public node_interface
	{
		using node_interface::node_interface;

		n3() {}

		void on_request_preamble(http::http_request&& message)
		{
			base::on_trailer({},{});
		}
	};

	auto c = make_unique_chain<node_interface,n1, n2, n3>();
    c->initialize_callbacks([](http::http_response &&) {}, [](dstring &&) {}, [](dstring &&, dstring &&) { ++count; },
                            []() {},
                            [](const errors::error_code &ec) {}, []() {});

	http::http_request msg{true};
	c->on_request_preamble(std::move(msg));
	ASSERT_EQ(count, 2);
}

TEST(cor_propagation, middle_error)
{
	count = 0;
	struct n1 : public node_interface
	{
		using node_interface::node_interface;

        n1() {}
    };

	struct n2 : public node_interface
	{
		using node_interface::node_interface;

        n2() {}

        void on_request_preamble(http::http_request&& message)
		{
			auto ciao = INTERNAL_ERROR(666);
			base::on_error(ciao);
		}
	};
	//if we pass by n3 in any way the test fails miserably.
	struct n3 : public node_interface
	{
		using node_interface::node_interface;

        n3() {}

        void on_request_preamble(http::http_request&& message) { count = 0;}
		void on_header(http::http_structured_data &&h) {count=0;}
		void on_body(dstring&& chunk){count = 0;}
		void on_error(const errors::error_code &ec) {count=0;}
		void on_end_of_message(){count=0;}
		void on_request_preamble_complete(){count=0; }

	};

	auto c = make_unique_chain<node_interface,n1, n2, n3, n3, n3, n3>();
    c->initialize_callbacks([](http::http_response &&) { count = 0; }, [](dstring &&) { count = 0; },
                            [](dstring &&, dstring &&) {}, []() { count = 0; },
                            [](const errors::error_code &ec) { ++count; }, []() {});

	http::http_request msg{true};
	c->on_request_preamble(std::move(msg));
	ASSERT_EQ(count, 1);
}

bool has_called_asynchronously = false;
TEST(cor_propagation, asynchronous_fw)
{
	local_io_service.reset();

	count = 0;
	struct n1 : public node_interface {
		using node_interface::node_interface;

        n1() {}

        void on_request_preamble(http::http_request&& message)
		{
			count = 1;
			base::on_request_preamble(std::move(message));
		}
	};

	struct n2 : public node_interface
	{
        n2() {}

        int ciao = 0;
		using node_interface::node_interface;
		void on_request_preamble(http::http_request&& message)
		{
			local_io_service.post([this, &message](){
				ASSERT_EQ(count, 1);
				has_called_asynchronously = true;
				base::on_request_preamble(std::move(message));
			});
		}
	};

	struct n3: public node_interface
	{
		using node_interface::node_interface;

        n3() {}

        void on_request_preamble(http::http_request&& message)
		{
			ASSERT_TRUE(has_called_asynchronously);
			ASSERT_EQ(count, 1);
			count++;
			base::on_request_preamble(std::move(message));
		}
	};

	auto c = make_unique_chain<node_interface,n1, n2, n3>();
    c->initialize_callbacks([](http::http_response &&) {}, [](dstring &&) {}, [](dstring &&, dstring &&) {}, []() {},
                            [](const errors::error_code &ec) {}, []() {});

	http::http_request msg{true};
	c->on_request_preamble(std::move(msg));

	local_io_service.run();
	ASSERT_EQ(count, 2);
	ASSERT_TRUE(has_called_asynchronously);
}

TEST(cor_propagation, asynchronous_bw)
{
	local_io_service.reset();
	has_called_asynchronously = false;
	count = 0;
	struct n1 : public node_interface {
		using node_interface::node_interface;

        n1() {}

        void on_request_preamble(http::http_request&& message)
		{
			count = 1;
			base::on_request_preamble(std::move(message));
		}
	};

	struct n2 : public node_interface
	{
        n2() {}

        int ciao = 0;
		using node_interface::node_interface;
		void on_request_preamble(http::http_request&& message)
		{
			local_io_service.post([this, &message]()
			{
				ASSERT_EQ(count, 1);
				base::on_request_preamble(std::move(message));
			});
		}

		void on_header(http::http_response&&res)
		{
			r = res;
			local_io_service.post([this]()
			{
				ASSERT_EQ(count, 2);
				has_called_asynchronously = true;
				base::on_header(std::move(r));
			});
		}

		http::http_response r;
	};

	struct n3: public node_interface
	{
		using node_interface::node_interface;

        n3() {}

        void on_request_preamble(http::http_request&& message)
		{
			ASSERT_EQ(count, 1);
			count++;
			base::on_request_preamble(std::move(message));
		}

        void on_request_finished()
        {
            on_header(http::http_response{});
        }
	};

	auto c = make_unique_chain<node_interface,n1, n2, n3>();
    c->initialize_callbacks([](http::http_response &&) {}, [](dstring &&) {}, [](dstring &&, dstring &&) {}, []() {},
                            [](const errors::error_code &ec) {}, []() {});

	http::http_request msg{true};
	c->on_request_preamble(std::move(msg));
	c->on_request_finished();

	local_io_service.run();
	ASSERT_EQ(count, 2);
	ASSERT_TRUE(has_called_asynchronously);
}

}

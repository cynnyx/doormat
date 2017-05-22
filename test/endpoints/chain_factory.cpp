#include <gtest/gtest.h>
#include <functional>
#include <memory>
#include "src/endpoints/chain_factory.h"
#include "src/http/http_request.h"
#include "src/chain_of_responsibility/node_interface.h"
#include "src/dummy_node.h"


TEST(chain_factory, get_endpoint) {
	using namespace endpoints;
	int fallback = 0;
	int proper = 0;
	auto cf = std::make_unique<chain_factory>([&fallback](){ ++fallback; return nullptr;});
	http::http_request dummy_request{};
	dummy_request.method(http_method::HTTP_GET);
	dummy_request.path("/prova");
	auto chain = cf->get_chain(dummy_request);
	ASSERT_FALSE(chain);
	ASSERT_EQ(fallback, 1);
	cf->get("*", "/prova", [&](){++proper; return nullptr;});
	auto chain2 = cf->get_chain(dummy_request);
	(void)chain2;
	ASSERT_EQ(proper, 1);
}

TEST(chain_factory, get_with_params) {
	using namespace endpoints;
	int fallback = 0;
	int proper = 0;
	auto cf = std::make_unique<chain_factory>([&fallback](){ ++fallback; return nullptr;});
	http::http_request dummy_request{};
	dummy_request.method(http_method::HTTP_GET);
	dummy_request.path("/prova/parameter");
	auto chain = cf->get_chain_and_params(dummy_request);
	ASSERT_FALSE(chain);
	ASSERT_EQ(fallback, 1);
	cf->get("*", "/prova/{p}", [&](){++proper; return nullptr;});
	auto chain2 = cf->get_chain_and_params(dummy_request);
	(void)chain2;
	ASSERT_TRUE(dummy_request.hasParameter("p"));
	ASSERT_TRUE(dummy_request.getParameter("p") == "parameter");
	ASSERT_EQ(proper, 1);
}

TEST(chain_factory, get_with_hosts) {
	using namespace endpoints;
	const auto host = "something.some";
	const auto path = "prova/bau";
	http::http_request dummy_request{};
	dummy_request.method(http_method::HTTP_GET);
	dummy_request.hostname(host);
	dummy_request.path(path);
	int fallback = 0;
	int proper = 0;
	auto chain_maker = [&](){ ++proper; return std::make_unique<dummy_node>(); };
	auto cf = std::make_unique<chain_factory>([&fallback](){ ++fallback; return nullptr;});

	cf->get(host, path, chain_maker);
	auto chain = cf->get_chain(dummy_request);
	EXPECT_TRUE(chain);
	EXPECT_EQ(fallback, 0);
	EXPECT_EQ(proper, 1);

	http::http_request dummy_request2{};
	dummy_request2.method(http_method::HTTP_GET);
	auto hn = std::string{host} + "z";
	dummy_request2.hostname(dstring{hn.data(), hn.size()});
	dummy_request2.path(path);
	auto chain2 = cf->get_chain(dummy_request2);
	EXPECT_FALSE(chain2);
	EXPECT_EQ(fallback, 1);
	EXPECT_EQ(proper, 1);
}


TEST(chain_factory, multiple_hosts) {
    using namespace endpoints;
    int fallback{0}, proper1{0}, proper2{0};
	auto chain_maker1 = [&](){ ++proper1; return std::make_unique<dummy_node>(); };
    auto chain_maker2 = [&](){ ++proper2; return std::make_unique<dummy_node>(); };
	auto cf = std::make_unique<chain_factory>([&fallback](){ ++fallback; return nullptr;});
    http::http_request dummy_request1{}, dummy_request2{};
    dummy_request1.method(http_method::HTTP_GET);
    dummy_request1.hostname("ciao.org");
    dummy_request1.path("/plain/aaaaaaaaaaaa");

    dummy_request2.method(http_method::HTTP_GET);
    dummy_request2.hostname("ciao.org");
    dummy_request2.path("/ai/aaaaaaaa");

	cf->get("*", "/plain/{url}", chain_maker1);
	EXPECT_NO_THROW(cf->get("*", "/ai/{url}", chain_maker2));

    auto ch1 = cf->get_chain(dummy_request1);
    auto ch2 = cf->get_chain(dummy_request2);
    EXPECT_TRUE(ch1); EXPECT_TRUE(ch2);
    EXPECT_EQ(fallback, 0);
    EXPECT_EQ(proper1, 1);
    EXPECT_EQ(proper2, 1);
}

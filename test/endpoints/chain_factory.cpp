#include <gtest/gtest.h>
#include <functional>
#include <memory>
#include "../../src/endpoints/chain_factory.h"
#include "../../src/http/http_request.h"
#include "../../src/chain_of_responsibility/node_interface.h"


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
	cf->get("", "/prova", [&](){++proper; return nullptr;});
	auto chain2 = cf->get_chain(dummy_request);
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
	cf->get("", "/prova/{p}", [&](){++proper; return nullptr;});
	auto chain2 = cf->get_chain_and_params(dummy_request);
	ASSERT_TRUE(dummy_request.hasParameter("p"));
	ASSERT_TRUE(dummy_request.getParameter("p") == "parameter");
	ASSERT_EQ(proper, 1);

}


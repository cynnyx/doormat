#include <gtest/gtest.h>
#include "../../src/requests_manager/cache_manager/cache_request_normalizer.h"
#include "common.h"

namespace
{
using namespace test_utils;
struct cache_normalizer_test : public preset::test
{
	virtual void SetUp() override
	{
		preset::test::SetUp();
		auto &rules = service::locator::configuration().get_cache_normalization_rules();

		if(rules.empty()) //fixme
		{
			std::vector<cache_normalization_rule> newrules;
			//apple transformation rule
			//variables defined are:
			newrules.emplace_back("^ww(.{1}).morphcast.video$", "^[\\/]?(apple-app-site-association|favicon.ico)$", "(.*)", "$<default>$");
			newrules.emplace_back("^ww(.{1}).morphcast.video$", "(.*)", "(facebot|facebookexternalhit|twitterbot|whatsapp|linkedinbot|skypeuripreview)", "$<default>$<header:user-agent>$");
			newrules.emplace_back("^ww(.{1}).morphcast.video$", "(.*)", "(.*)", "$www.morphcast.video$/index.html$$");
			service::locator::configuration().set_normalization_rules(newrules);
		}
	}

};

}

TEST_F(cache_normalizer_test, fixed_normalizer_test)
{
	http::http_request req;
	req.hostname("ww1.morphcast.video");
	req.path("ciaone");
	req.query("ariciaone");
	auto ch = make_unique_chain<node_interface, first_node, nodes::cache_request_normalizer, last_node>();
	ch->on_request_preamble(std::move(req));
	ch->on_request_finished();

	ASSERT_TRUE(last_node::request.is_initialized());
	ASSERT_TRUE(last_node::request->has("x-custom-cache-key"));
	std::string ckey = last_node::request->header("x-custom-cache-key");
	ASSERT_EQ(ckey, "$www.morphcast.video$/index.html$$");

}



TEST(cache_normalizer_test2, norm1)
{
	cache_normalization_rule rule{"^ww(.{1}).morphcast.video$", "(.*)", "(.*)", "$www.morphcast.video$/index.html$$"};
	http::http_request req;
	req.hostname("ww1.morphcast.video");
	req.path("ciaone");
	req.query("ariciaone");
	if(rule.matches(req))
	{
		std::string ckey = rule.get_cache_key(req);
		ASSERT_EQ(ckey, "$www.morphcast.video$/index.html$$");


		return;
	}
	FAIL() << "rule did not matched.";
}


TEST_F(cache_normalizer_test, ua_test)
{
	http::http_request req;
	req.hostname("ww1.morphcast.video");
	req.path("something");
	req.header("user-agent", "facebot");
	req.query("ariciaone");
	auto ch = make_unique_chain<node_interface, first_node, nodes::cache_request_normalizer, last_node>();
	ch->on_request_preamble(std::move(req));
	ch->on_request_finished();

	ASSERT_TRUE(last_node::request.is_initialized());
	ASSERT_TRUE(last_node::request->has("x-custom-cache-key"));
	std::string ckey = last_node::request->header("x-custom-cache-key");
	ASSERT_EQ(ckey, "$ww1.morphcast.video$something$ariciaone$facebot$");
}


TEST_F(cache_normalizer_test, path_test)
{
	http::http_request req;
	req.hostname("ww1.morphcast.video");
	req.path("apple-app-site-association");
	req.header("user-agent", "Chrome");
	req.query("ciaone=boh");
	auto ch = make_unique_chain<node_interface, first_node, nodes::cache_request_normalizer, last_node>();
	ch->on_request_preamble(std::move(req));
	ch->on_request_finished();

	ASSERT_TRUE(last_node::request.is_initialized());
	ASSERT_TRUE(last_node::request->has("x-custom-cache-key"));
	std::string ckey = last_node::request->header("x-custom-cache-key");
	ASSERT_EQ(ckey, "$ww1.morphcast.video$apple-app-site-association$ciaone=boh$");

}


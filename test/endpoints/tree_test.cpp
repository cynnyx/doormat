#include <gtest/gtest.h>
#include "../../src/endpoints/path/radix_tree.h"
#include "../../src/chain_of_responsibility/node_interface.h"
#include "../src/endpoints/generator.h"

using radix_tree_path = endpoints::radix_tree<endpoints::chain_generator_t, '/', false>;
using radix_tree_host = endpoints::radix_tree<endpoints::tree_generator_t, '.', true>;

TEST(radix_tree_path, duplicate_path_fails) {
	int count = 0;
	auto root = std::make_unique<radix_tree_path>();
	root->addPattern("/test/", [&count]()
	{
		++count;
		return nullptr;
	});
	EXPECT_THROW(root->addPattern("/test/", [&count]()
	{
		++count;
		return nullptr;
	}), std::invalid_argument);
	EXPECT_THROW(root->addPattern("test/", [&count]()
	{
		++count;
		return nullptr;
	}), std::invalid_argument);
	EXPECT_THROW(root->addPattern("/test", [&count]()
	{
		++count;
		return nullptr;
	}), std::invalid_argument);
}

TEST(radix_tree_path, duplicate_complex_path_fails) {
	int count = 0;
	auto root = std::make_unique<radix_tree_path>();
	root->addPattern("/test/*/ciaone/", [&count]()
	{
		++count;
		return nullptr;
	});
	EXPECT_THROW(root->addPattern("test/*/ciaone", [&count]()
	{
		++count;
		return nullptr;
	}), std::invalid_argument);
	EXPECT_THROW(root->addPattern("/test/*/ciaone", [&count]()
	{
		++count;
		return nullptr;
	}), std::invalid_argument);
}

TEST(radix_tree_path, shortest_path_last_works) {
	auto root = std::make_unique<radix_tree_path>();
	root->addPattern("/prova/di/path/specifico", []()
	{ return nullptr; });
	EXPECT_NO_THROW(root->addPattern("/prova/", []()
	{ return nullptr; }));
}

TEST(radix_tree_path, append_path_works) {
	auto root = std::make_unique<radix_tree_path>();
	root->addPattern("/prova/di/path/specifico", []()
	{ return nullptr; });
	EXPECT_NO_THROW(root->addPattern("/prova/di/path/ancora/piu/specifico", []()
	{ return nullptr; }));
}

TEST(radix_tree_path, wildcard_matching) {
	auto root = std::make_unique<radix_tree_path>();
	root->addPattern("/ai/*", [](){ return nullptr; });
	EXPECT_TRUE(root->matches("/ai/http://prova.com/image.jpg?q=prova"));
}

TEST(radix_tree_path, parameter_matching) {
	auto root = std::make_unique<radix_tree_path>();
	root->addPattern("/ai/{imgUrl}/saliency/something", [](){ return nullptr; });
	EXPECT_TRUE(root->matches("/ai/thisisafakeparameter/saliency/something"));
	EXPECT_FALSE(root->matches("/ai/thisisafakeparameter/ciao/ciccio"));
	EXPECT_FALSE(root->matches("/ai/saluda/andonio"));
	EXPECT_FALSE(root->matches("/ai/thisisafakeparameter/saliency/something/else"));
}

TEST(radix_tree_path, more_specific_matching) {
	auto root = std::make_unique<radix_tree_path>();
	root->addPattern("/ai/*/prova", [](){ return nullptr; });
	EXPECT_FALSE(root->matches("/ai/ciaone"));
	EXPECT_TRUE(root->matches("/ai/ciaone/prova"));
	EXPECT_FALSE(root->matches("/ai/saluda/andonio/prova"));
	EXPECT_FALSE(root->matches("/ai/ciaone/prova/2"));
}


TEST(radix_tree_host, duplicate_host_fails) {
	int count = 0;
	const auto host = "sys.sos.sus";
	auto root = std::make_unique<radix_tree_host>();
	root->addPattern(host, [&count]()
	{
		++count;
		return nullptr;
	});
	EXPECT_THROW(root->addPattern(host, [&count]()
	{
		++count;
		return nullptr;
	}), std::invalid_argument);
	EXPECT_THROW(root->addPattern(host, [&count]()
	{
		++count;
		return nullptr;
	}), std::invalid_argument);
	EXPECT_THROW(root->addPattern(host, [&count]()
	{
		++count;
		return nullptr;
	}), std::invalid_argument);
}

TEST(radix_tree_host, duplicate_complex_host_fails) {
	int count = 0;
	const auto host = "sys.*.sos";
	auto root = std::make_unique<radix_tree_host>();
	root->addPattern(host, [&count]()
	{
		++count;
		return nullptr;
	});
	EXPECT_THROW(root->addPattern(host, [&count]()
	{
		++count;
		return nullptr;
	}), std::invalid_argument);
	EXPECT_THROW(root->addPattern(host, [&count]()
	{
		++count;
		return nullptr;
	}), std::invalid_argument);
}

TEST(radix_tree_host, shortest_path_last_works) {
	auto root = std::make_unique<radix_tree_host>();
	root->addPattern("cy1.sys.cynny.com", []()
	{ return nullptr; });
	EXPECT_NO_THROW(root->addPattern("cynny.com", []()
	{ return nullptr; }));
}

TEST(radix_tree_host, append_host_works) {
	auto root = std::make_unique<radix_tree_host>();
	root->addPattern("specific.host.test.com", []()
	{ return nullptr; });
	EXPECT_NO_THROW(root->addPattern("very.specific.host.test.com", []()
	{ return nullptr; }));
}

TEST(radix_tree_host, wildcard_matching) {
	auto root = std::make_unique<radix_tree_host>();
	root->addPattern("*.cynny.com", [](){ return nullptr; });
	EXPECT_TRUE(root->matches("cy1.sys.cynny.com"));
}

TEST(radix_tree_host, parameter_matching) {
	auto root = std::make_unique<radix_tree_host>();
	root->addPattern("cy1.{subdomain}.sys.cynny.com", [](){ return nullptr; });
	EXPECT_TRUE(root->matches("cy1.thisisafakeparameter.sys.cynny.com"));
	EXPECT_FALSE(root->matches("cy1.thisisafakeparameter.sys.ciao.com"));
	EXPECT_FALSE(root->matches("cy1.ciupa.ciups"));
	EXPECT_FALSE(root->matches("another.cy1.thisisafakeparameter.sys.cynny.com"));
}

TEST(radix_tree_host, more_specific_matching) {
	auto root = std::make_unique<radix_tree_path>();
	root->addPattern("/ai/*/prova", [](){ return nullptr; });
	EXPECT_FALSE(root->matches("/ai/ciaone"));
	EXPECT_TRUE(root->matches("/ai/ciaone/prova"));
	EXPECT_FALSE(root->matches("/ai/saluda/andonio/prova"));
	EXPECT_FALSE(root->matches("/ai/ciaone/prova/2"));
}

#include <gtest/gtest.h>
#include "../../src/endpoints/tree/tree.h"
#include "../../src/chain_of_responsibility/node_interface.h"

TEST(endpoints, duplicate_path_fails) {
    using namespace endpoints;
    int count = 0;
    auto root = std::make_unique<tree>("");
    root->addPattern("/test/", [&count]()
    {
        ++count;
        return std::unique_ptr<node_interface>{nullptr};
    });
    EXPECT_THROW(root->addPattern("/test/", [&count]()
    {
        ++count;
        return std::unique_ptr<node_interface>{nullptr};
    }), std::invalid_argument);
    EXPECT_THROW(root->addPattern("test/", [&count]()
    {
        ++count;
        return std::unique_ptr<node_interface>{nullptr};
    }), std::invalid_argument);
    EXPECT_THROW(root->addPattern("/test", [&count]()
    {
        ++count;
        return std::unique_ptr<node_interface>{nullptr};
    }), std::invalid_argument);
}

TEST(endpoints, duplicate_complex_path_fails) {
    using namespace endpoints;
    int count = 0;
    auto root = std::make_unique<tree>("");
    root->addPattern("/test/*/ciaone/", [&count]()
    {
        ++count;
        return std::unique_ptr<node_interface>{nullptr};
    });
    EXPECT_THROW(root->addPattern("test/*/ciaone", [&count]()
    {
        ++count;
        return std::unique_ptr<node_interface>{nullptr};
    }), std::invalid_argument);
    EXPECT_THROW(root->addPattern("/test/*/ciaone", [&count]()
    {
        ++count;
        return std::unique_ptr<node_interface>{nullptr};
    }), std::invalid_argument);
}

TEST(endpoints, shortest_path_last_works) {
    using namespace endpoints;
    auto root = std::make_unique<tree>("");
    root->addPattern("/prova/di/path/specifico", []()
    { return std::unique_ptr<node_interface>{nullptr}; });
    EXPECT_NO_THROW(root->addPattern("/prova/", []()
    { return std::unique_ptr<node_interface>{nullptr}; }));
}


TEST(endpoints, append_path_works) {
    using namespace endpoints;
    auto root = std::make_unique<tree>("");
    root->addPattern("/prova/di/path/specifico", []()
    { return std::unique_ptr<node_interface>{nullptr}; });
    EXPECT_NO_THROW(root->addPattern("/prova/di/path/ancora/piu/specifico", []()
    { return std::unique_ptr<node_interface>{nullptr}; }));
}

TEST(endpoints, wildcard_matching) {
    using namespace endpoints;
    auto root = std::make_unique<tree>("");
    root->addPattern("/ai/*", [](){ return std::unique_ptr<node_interface>{nullptr}; });
    EXPECT_TRUE(root->matches("/ai/http://prova.com/image.jpg?q=prova"));
}

TEST(endpoints, parameter_matching) {
    using namespace endpoints;
    auto root = std::make_unique<tree>("");
    root->addPattern("/ai/{imgUrl}/saliency/something", [](){ return std::unique_ptr<node_interface>{nullptr}; });
    EXPECT_TRUE(root->matches("/ai/thisisafakeparameter/saliency/something"));
    EXPECT_FALSE(root->matches("/ai/thisisafakeparameter/ciao/ciccio"));
    EXPECT_FALSE(root->matches("/ai/saluda/andonio"));
    EXPECT_FALSE(root->matches("/ai/thisisafakeparameter/saliency/something/else"));
}
TEST(endpoints, more_specific_matching) {
    using namespace endpoints;
    auto root = std::make_unique<tree>("");
    root->addPattern("/ai/*/prova", [](){ return std::unique_ptr<node_interface>{nullptr}; });
    EXPECT_FALSE(root->matches("/ai/ciaone"));
    EXPECT_TRUE(root->matches("/ai/ciaone/prova"));
    EXPECT_FALSE(root->matches("/ai/saluda/andonio/prova"));
    EXPECT_FALSE(root->matches("/ai/ciaone/prova/2"));
}
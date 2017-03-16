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

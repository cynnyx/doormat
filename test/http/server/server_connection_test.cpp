#include "../src/http/server/server_connection.h"
#include "../src/protocol/handler_http1.h"
#include "../src/http/server/server_traits.h"
#include "mocks/mock_connector.h"
#include <gtest/gtest.h>
#include <gmock/gmock.h>

using namespace http;

TEST(server_connection, invalid_request) {
    MockConnector::wcb cb = [](dstring chunk){};
    auto handler_test = std::make_shared<server::handler_http1<http::server_traits>>(http::proto_version::HTTP11);
    auto conn = std::make_shared<MockConnector>(cb);
    conn->handler(handler_test);

    bool ok{false};
    handler_test->on_request([&ok](auto conn, auto req, auto resp) {
        req->on_error([&ok](auto conn, auto &error){
            ASSERT_TRUE(conn);
            ASSERT_EQ(error.errc(), http::error_code::decoding);
            ok = true;
        });
    });

    conn->io_service().post([conn](){ conn->read("GET /prova HTTP_WRONG/1.1"); });
    conn->io_service().run();

    ASSERT_TRUE(ok);
}

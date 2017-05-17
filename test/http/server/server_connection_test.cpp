#include "../src/http/server/server_connection.h"
#include "../src/protocol/handler_http1.h"
#include "../src/http/server/server_traits.h"
#include "mocks/mock_connector.h"
#include <gtest/gtest.h>
#include <gmock/gmock.h>

using namespace http;

TEST(server_connection, invalid_request) {
//    auto cb = [](){};
//    auto handler_test = std::make_shared<server::handler_http1<http::server_traits>>(http::proto_version::HTTP11);
//    auto conn = std::make_shared<MockConnector>(cb);

//    bool ok{false};
//    handler_test->on_request([&ok](auto conn, auto req, auto resp) {
//        req->on_error([&ok](auto conn, auto &error){
//            ASSERT_TRUE(conn);
//            ASSERT_EQ(error, http::error_code::decoding);
//            ok = true;
//        });
//    });

//    conn->io_service().post([conn](){ conn->read("GET /prova HT5P/1.1"); });
//    conn->io_service().run();

//    ASSERT_TRUE(ok);

}

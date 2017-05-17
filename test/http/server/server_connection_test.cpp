#include "../src/http/server/server_connection.h"
#include <gtest/gtest.h>
#include <gmock/gmock.h>

using namespace http;

//TEST(server_connection, invalid_request_cb) {
//    auto cb = [](){};
//    auto handler_test = std::make_shared<server::handler_http1>(http::proto_version::HTTP11);

//    auto m = std::make_shared<MockConnector>(cb);

//    bool ok{false};
//    handler_test->on_request_received([](auto req, auto resp) {
//        req->on_error([](auto conn, auto &error){
//            ASSERT_TRUE(conn);
//            ASSERT_EQ(error, http::error_code::decoding);
//            ok = true;
//        });
//    });

//    m->io_service().post([m](){    m->read("GET /prova HT5P/1.1"); });
//    m->io_service().run();

//    ASSERT_TRUE(ok);

//}

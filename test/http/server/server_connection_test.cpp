#include "../src/http/server/server_connection.h"
#include "../src/protocol/handler_http1.h"
#include "../src/http/server/server_traits.h"
#include "mocks/mock_connector.h"
#include <gtest/gtest.h>
#include <gmock/gmock.h>

using namespace http;

TEST(server_connection, invalid_request) {
    boost::asio::io_service io;
    MockConnector::wcb cb = [](dstring chunk){};
    auto handler_test = std::make_shared<server::handler_http1<http::server_traits>>(http::proto_version::HTTP11);
    auto connector = std::make_shared<MockConnector>(io, cb);
    connector->handler(handler_test);

    bool ok{false};
    handler_test->on_request([&ok](auto conn, auto req, auto resp) {
        req->on_error([&ok](auto conn, auto &error){
            ASSERT_TRUE(conn);
            ASSERT_EQ(error.errc(), http::error_code::decoding);
            ok = true;
        });
    });

    connector->io_service().post([connector](){ connector->read("GET /prova HTTP_WRONG!!!/1.1"); });
    connector->io_service().run();

    ASSERT_TRUE(handler_test->start());
    ASSERT_TRUE(ok);
}


TEST(server_connection, valid_request) {
    boost::asio::io_service io;

    MockConnector::wcb cb = [](dstring chunk){};
    auto handler_test = std::make_shared<server::handler_http1<http::server_traits>>(http::proto_version::HTTP11);
    auto connector = std::make_shared<MockConnector>(io, cb);
    connector->handler(handler_test);
    size_t expected_cb_counter{2};

    size_t cb_counter{0};
    handler_test->on_request([&cb_counter](auto conn, auto req, auto resp) {
        req->on_headers([&cb_counter](auto req){
            ++cb_counter;
        });

        req->on_finished([&cb_counter](auto req){
            ++cb_counter;
        });
    });

    connector->io_service().post([connector](){ connector->read("GET / HTTP/1.1\r\n"
                                                                "host:localhost:1443\r\n"
                                                                "date: Tue, 17 May 2016 14:53:09 GMT\r\n"
                                                                "\r\n"); });
    connector->io_service().run();
    ASSERT_TRUE(handler_test->start());
    ASSERT_EQ(cb_counter, expected_cb_counter);
}


TEST(server_connection, on_connector_nulled) {
    boost::asio::io_service io;

    MockConnector::wcb cb = [](dstring chunk){};
    auto handler_test = std::make_shared<server::handler_http1<http::server_traits>>(http::proto_version::HTTP11);
    auto connector = std::make_shared<MockConnector>(io, cb);
    connector->handler(handler_test);
    size_t expected_cb_counter{2};

    size_t cb_counter{0};
    bool ok{false};


    handler_test->on_request([&cb_counter, &ok, &connector](auto conn, auto req, auto resp) {


        req->on_headers([&cb_counter, &connector](auto req){
            ++cb_counter;

        });

        req->on_finished([&cb_counter, &conn, &connector, resp](auto req){
            ++cb_counter;
            connector = nullptr;
            resp->headers(http::http_response{});
        });

        conn->on_error([](auto conn, const http::connection_error &error){
            std::cout << "connection on error" << std::endl;
        });

        req->on_error([&ok](auto conn, auto &error){
            ASSERT_TRUE(conn);
            ASSERT_EQ(error.errc(), http::error_code::closed_by_client);
            ok = true;
        });
/*
        resp->on_error([](auto conn){

        });*/
    });

    connector->io_service().post([connector](){
        connector->read("GET / HTTP/1.1\r\n"
                        "host:localhost:1443\r\n"
                        "date: Tue, 17 May 2016 14:53:09 GMT\r\n"
                        "\r\n");
    });

    connector->io_service().run();
    ASSERT_EQ(cb_counter, expected_cb_counter);
    ASSERT_TRUE(ok);
}



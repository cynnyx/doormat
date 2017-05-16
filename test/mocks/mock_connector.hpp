#ifndef DOOR_MAT_MOCK_SERVER_H
#define DOOR_MAT_MOCK_SERVER_H

#include "../src/connector.h"

struct MockConnector : public server::connector_interface
{
    using wcb = std::function<void(void)>;
    wcb& write_cb;
    boost::asio::io_service io;

    MockConnector(wcb& cb): write_cb(cb), io{} {}
    void do_write() override { write_cb(); }
    void do_read() override {}
    void close() override {}
    boost::asio::ip::address origin() const override { return boost::asio::ip::address::from_string("127.0.0.1");}
    bool is_ssl() const noexcept override { return true; }
    void set_timeout(std::chrono::milliseconds) override {}
    boost::asio::io_service &io_service() { return io; }
};

#endif //DOOR_MAT_MOCK_SERVER_H


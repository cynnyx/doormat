#pragma once

#include "../src/connector.h"

struct MockConnector : server::connector_interface
{
    using wcb = std::function<void(void)>;

    MockConnector(wcb& cb);
    void do_write() override;
    void do_read() override;
    void close() override;
    boost::asio::ip::address origin() const override;
    bool is_ssl() const noexcept override;
    void set_timeout(std::chrono::milliseconds) override;
    boost::asio::io_service &io_service();
	void handler(std::shared_ptr<server::http_handler>) override;
	void start(bool) override;

    wcb& write_cb;
    boost::asio::io_service io;
};

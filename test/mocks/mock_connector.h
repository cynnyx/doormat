#pragma once

#include "../src/connector.h"
#include "../src/protocol/http_handler.h"
#include <memory>

struct MockConnector : server::connector_interface, std::enable_shared_from_this<MockConnector>
{
    using wcb = std::function<void(dstring)>;

    MockConnector(wcb& cb);

    void do_write() override;
    void do_read() override;
    boost::asio::ip::address origin() const override;
    bool is_ssl() const noexcept override;
    void close() override;
    boost::asio::io_service &io_service() override;
    void set_timeout(std::chrono::milliseconds) override;
	void read(std::string request);
	void handler(std::shared_ptr<server::http_handler> h);

    wcb& write_cb;
    std::shared_ptr<server::http_handler> _handler;
    boost::asio::io_service io;
};

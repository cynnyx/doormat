
#include "../src/connector.h"
#include "mock_connector.h"

MockConnector::MockConnector(wcb& cb)
    : write_cb(cb), io{}
{}

void MockConnector::do_write()
{
    write_cb();
}

void MockConnector::do_read()
{}

void MockConnector::close()
{}

boost::asio::ip::address MockConnector::origin() const
{
    return boost::asio::ip::address::from_string("127.0.0.1");
}

bool MockConnector::is_ssl() const noexcept
{
    return true;
}

void MockConnector::set_timeout(std::chrono::milliseconds)
{}

boost::asio::io_service& MockConnector::io_service()
{
    return io;
}

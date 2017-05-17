
#include "../src/connector.h"
#include "mock_connector.h"

MockConnector::MockConnector(wcb& cb)
    : write_cb(cb), _handler{nullptr}, io{}
{}

void MockConnector::do_write()
{
    dstring chunk;
    _handler->on_write(chunk);
    write_cb(chunk);
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

void MockConnector::read(std::string request)
{
    _handler->on_read(request.data(), request.size());
}

void MockConnector::handler(std::shared_ptr<server::http_handler> h)
{
	_handler = std::move(h);
	_handler->connector(this->shared_from_this());
	_handler->start();
}

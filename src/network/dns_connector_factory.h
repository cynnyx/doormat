#ifndef DOORMAT_DNS_CONNECTOR_FACTORY_H
#define DOORMAT_DNS_CONNECTOR_FACTORY_H

#include "connector_factory.h"
#include <boost/asio.hpp>
namespace network
{

class dns_connector_factory : public connector_factory
{
public:
    void get_connector(const http::http_request &, connector_callback_t, error_callback_t) override;
    void stop() override { stopping = true; };
    virtual ~dns_connector_factory() = default;

private:
    void dns_resolver(const http::http_request &req, connector_callback_t, error_callback_t);
    void endpoint_connect(boost::asio::ip::tcp::resolver::iterator, std::shared_ptr<boost::asio::ip::tcp::socket>, connector_callback_t, error_callback_t);
    bool stopping{false};
    static constexpr int resolve_timeout = 2000;
    static constexpr int connect_timeout = 1000;
};

}

#endif //DOORMAT_DNS_CONNECTOR_FACTORY_H

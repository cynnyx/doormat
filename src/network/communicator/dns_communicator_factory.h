#ifndef DOORMAT_DNS_CONNECTOR_FACTORY_H
#define DOORMAT_DNS_CONNECTOR_FACTORY_H

#include "communicator_factory.h"
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

namespace network
{

class dns_communicator_factory : public communicator_factory
{
public:
	void get_connector(const std::string& address, uint16_t port, bool tls, connector_callback_t, error_callback_t) override;
	void stop() override { stopping = true; }

private:
	void dns_resolver(const std::string& address, uint16_t port, bool tls, connector_callback_t, error_callback_t);
	//template<typename T> // boost::asio::ip::tcp::socket
	void endpoint_connect(boost::asio::ip::tcp::resolver::iterator, std::shared_ptr<boost::asio::ip::tcp::socket>, 
		connector_callback_t, error_callback_t);
	void endpoint_connect(boost::asio::ip::tcp::resolver::iterator,
		std::shared_ptr<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>>, 
		connector_callback_t, error_callback_t);
	bool stopping{false};
	static constexpr int resolve_timeout = 2000;
	static constexpr int connect_timeout = 1000;
};

}

#endif //DOORMAT_DNS_CONNECTOR_FACTORY_H

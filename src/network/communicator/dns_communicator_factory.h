#ifndef DOORMAT_DNS_CONNECTOR_FACTORY_H
#define DOORMAT_DNS_CONNECTOR_FACTORY_H

#include "communicator_factory.h"

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

#include <memory>
#include <chrono>

namespace network
{

class dns_connector_factory : public connector_factory
{
public:
	dns_connector_factory(boost::asio::io_service &io, std::chrono::milliseconds connector_timeout);
	~dns_connector_factory();

	void get_connector(const std::string& address, uint16_t port, bool tls, connector_callback_t, error_callback_t) override;
	void stop() override { stopping = true; }

private:
	using ssl_socket_t = boost::asio::ssl::stream<boost::asio::ip::tcp::socket>;

	void dns_resolver(const std::string& address, uint16_t port, bool tls, connector_callback_t, error_callback_t);
	//template<typename T> // boost::asio::ip::tcp::socket
	void endpoint_connect(boost::asio::ip::tcp::resolver::iterator, std::shared_ptr<boost::asio::ip::tcp::socket>, 
		connector_callback_t, error_callback_t);
	void endpoint_connect(boost::asio::ip::tcp::resolver::iterator,
		std::shared_ptr<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>>, 
		connector_callback_t, error_callback_t);

	static boost::asio::ssl::context init_ssl_ctx();
	static http::proto_version chose_protocol(std::shared_ptr<ssl_socket_t> stream);

	bool stopping{false};
	boost::asio::io_service &io;
	std::chrono::milliseconds conn_timeout;
	// ugly workaround to ensure in callbacks that we are still alive
	std::shared_ptr<bool> dead;


	static thread_local boost::asio::ssl::context ctx;
	static constexpr int resolve_timeout = 2000;
	static constexpr int connect_timeout = 1000;
};

}

#endif //DOORMAT_DNS_CONNECTOR_FACTORY_H

#ifndef CLOUDIA_POOL_H
#define CLOUDIA_POOL_H

#include <type_traits>

#include "socket_factory.h"

#include <boost/asio.hpp>

namespace network
{
	
// Ok, this can't be really a pool.
// Missing TLS support
class cloudia_pool: public socket_factory
{
	bool stopping{false};
	std::unique_ptr<socket_type> _socket;
	error_callback on_error_cb;
	socket_callback on_socket;
	boost::asio::deadline_timer _deadline;
	boost::asio::ip::tcp::resolver::iterator it;

	void init_socket();
	void handle_connect( const boost::asio::ip::tcp::endpoint& endpoint, socket_callback cb );
	void retry( socket_callback cb );

	void set_timeout();
	void on_error();
public:
	cloudia_pool();
	void stop() override;
	virtual ~cloudia_pool();
	
	/** \brief gets a socket and returns it to the user
	 *  \return a unique_ptr encapsulating a (hopefully valid) socket.
	 */
	void get_socket ( socket_callback cb ) override;
	void get_socket( const http::http_request &req, socket_callback sc, error_callback ec = [](){} ) override;
	bool set_destination( const std::string& ip, std::uint16_t port_, bool ssl = false ) override;
	void set_error( error_callback ec ) override { on_error_cb = ec; }
};

class cloudia_pool_factory : public abstract_factory_of_socket_factory
{
public:
	std::unique_ptr<socket_factory> get_socket_factory( std::size_t size = 0 ) const override;
};

}
#endif // CLOUDIA_POOL_H

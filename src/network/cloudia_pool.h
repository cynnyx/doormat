#ifndef CLOUDIA_POOL_H
#define CLOUDIA_POOL_H

#include <type_traits>
#include <memory>

#include "socket_factory.h"

#include <boost/asio.hpp>

namespace network
{

//Enable shared from this?
// Ok, this can't be really a pool.
// Missing TLS support
class cloudia_pool: public socket_factory<boost::asio::ip::tcp::socket>
{
	struct connection_handler: public std::enable_shared_from_this<connection_handler>
	{
		cloudia_pool* cp_p;
		
		std::unique_ptr<socket_type> _socket;
		error_callback on_error_cb;
		socket_callback on_socket;
		boost::asio::deadline_timer _deadline;
		boost::asio::ip::tcp::resolver::iterator it;
		
		connection_handler( cloudia_pool* out );
		connection_handler( const connection_handler& ) = delete;
		connection_handler( connection_handler&& ) = delete;
		connection_handler& operator=( const connection_handler& ) = delete;
		connection_handler& operator=( connection_handler&& ) = delete;
		
		void init_socket();
		void handle_connect( const boost::asio::ip::tcp::endpoint& endpoint, socket_callback cb );
		void retry( socket_callback cb );
		void set_timeout();
	};
	std::shared_ptr<connection_handler> handler;
	bool stopping{false};

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
	void set_error( error_callback ec ) override { if (handler) handler->on_error_cb = ec; }
	
	cloudia_pool( const cloudia_pool& ) = delete;
	cloudia_pool& operator=( const cloudia_pool& ) = delete;
	cloudia_pool( cloudia_pool&& ) = delete;
	cloudia_pool& operator=( cloudia_pool&& ) = delete;
};

class cloudia_pool_factory : public abstract_factory_of_socket_factory<boost::asio::ip::tcp::socket>
{
public:
	std::unique_ptr<socket_factory<boost::asio::ip::tcp::socket>> get_socket_factory( std::size_t size = 0 ) const override;
};

}
#endif // CLOUDIA_POOL_H

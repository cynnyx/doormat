#ifndef SOCKET_FACTORY_H
#define SOCKET_FACTORY_H

#include <memory>
#include <chrono>

#include <boost/asio.hpp>
#include "../http/http_request.h"

namespace network
{
	
class socket_factory
{
public:
	virtual ~socket_factory() {}
	
	using socket_type = boost::asio::ip::tcp::socket;
	using socket_callback = std::function<void(std::unique_ptr<socket_type>)>;
// 	using socket_callback_representation = std::pair<socket_callback , std::chrono::system_clock::time_point>;
	
	/** \brief analyzes the request and returns a socket accordingly.
	 * \param req the request representation
	 * \param socket_callback callback to be called when the socket is ready or there's an error
	 * */
	virtual void get_socket(const http::http_request &req, socket_callback sc) = 0;
	
	/** \brief returns a socket without caring of optimizations
	 *  \param socket_callback the callback to return the socket to the caller
	 * */
	virtual void get_socket(socket_callback sc) = 0;
	
	/** \brief gets a socket to the specified destination and returns it to the user
	 *
	 * \param address the address required
	 * \param cb the callback to call with the connected socket
	 * 	return a unique_ptr encapsulating a valid socket to the destination required.
	 * */
	virtual void get_socket(const routing::abstract_destination_provider::address &address, 
		socket_callback sc) = 0;
	
	virtual void stop() = 0;
};

class abstract_factory_of_socket_factory
{
public:
	virtual std::unique_ptr<socket_factory> get_socket_factory( std::size_t size ) const = 0;
	virtual ~abstract_factory_of_socket_factory() = default;
};

}
#endif // SOCKET_FACTORY_H

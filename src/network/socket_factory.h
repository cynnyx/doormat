#ifndef SOCKET_FACTORY_H
#define SOCKET_FACTORY_H

#include <memory>
#include <chrono>

#include <boost/asio.hpp>
#include "../http/http_request.h"

namespace network
{

template<class connection = boost::asio::ip::tcp::socket>
class socket_factory : public std::enable_shared_from_this<socket_factory<connection>>
{
public:
	virtual ~socket_factory() = default;
	using socket_type = connection;
	using socket_callback = std::function<void(std::unique_ptr<socket_type>)>;
	using error_callback = std::function<void()>;
// 	using socket_callback_representation = std::pair<socket_callback , std::chrono::system_clock::time_point>;
	
	/** \brief analyzes the request and returns a socket accordingly.
	 * \param req the request representation
	 * \param socket_callback callback to be called when the socket is ready or there's an error
	 * */
	virtual void get_socket(const http::http_request &req, socket_callback sc, error_callback ec = [](){}) = 0;
	
	/** \brief returns a socket without caring of optimizations
	 *  \param socket_callback the callback to return the socket to the caller
     * 
     *  Useful to implement a socket choosing algorithm that does not need a request.
	 *  Deprecated?
	 * */
	virtual void get_socket(socket_callback sc) = 0;
	virtual bool set_destination( const std::string& ip, std::uint16_t port_, bool ssl = false ) = 0;
	virtual void set_error( error_callback ec ) = 0;

	virtual void stop() = 0;
};

template<class socket_type = boost::asio::ip::tcp::socket>
class abstract_factory_of_socket_factory
{
public:
	virtual std::unique_ptr<socket_factory<socket_type>> get_socket_factory( std::size_t size ) const = 0;
	virtual ~abstract_factory_of_socket_factory() = default;
};

}
#endif // SOCKET_FACTORY_H

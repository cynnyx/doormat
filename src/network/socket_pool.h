#ifndef DOORMAT_SOCKET_POOL_H
#define DOORMAT_SOCKET_POOL_H

#include <memory>
#include <queue>
#include <chrono>
#include <list>
#include <unordered_map>

#include <boost/asio.hpp>
#include "../board/abstract_destination_provider.h"

#include "socket_factory.h"

/**
 * \brief the socket pool is the component responsible of providing fresh sockets to contact the boards.
 */

namespace network
{

class socket_pool: public socket_factory
{
public:
	using socket_callback_representation = std::pair<socket_callback , std::chrono::system_clock::time_point>;

	socket_pool(size_t starting_boards);

	/** \brief gets a socket and returns it to the user
	 *  \return a unique_ptr encapsulating a (hopefully valid) socket.
	 */
	void get_socket ( socket_callback cb ) override;
	void get_socket( const http::http_request &req, socket_pool::socket_callback socket_callback ) override;
	void stop() override;
	virtual ~socket_pool();
	
	/** \brief gets a socket to the specified destination and returns it to the user
	 *
	 * \param address the address required
	 * \param cb the callback to call with the connected socket
	 * 	return a unique_ptr encapsulating a valid socket to the destination required.
	 * */
	void get_socket(const routing::abstract_destination_provider::address &address, socket_callback cb) override;
private:

	
	void generate_socket();
	void append_socket(std::shared_ptr<socket_type> socket, const std::chrono::seconds &duration, const routing::abstract_destination_provider::address &addr);
	void renew_anysocket_timer();
	void notify_connect_timeout();
	void increase_pool_size();
	void decrease_pool_size();

	struct socket_representation
	{
		socket_representation(std::weak_ptr<socket_type> socket, const std::chrono::seconds &duration);

		bool is_valid()
		{
			return std::chrono::system_clock::now() < expiration && !socket.expired();
		}

		~socket_representation() = default;

		std::chrono::system_clock::time_point expiration;
		std::weak_ptr<socket_type> socket;
	};
	size_t required_board_size;

	std::unordered_map<routing::abstract_destination_provider::address, std::queue<socket_representation>> socket_queues;

	boost::asio::deadline_timer socketcb_expiration;
	std::queue<socket_callback_representation> anysocket_cb;

	const static std::chrono::seconds socket_expiration;
	const static std::chrono::seconds request_expiration;
	bool stopping{false};
	size_t connection_attempts{0};
	char fake_read_buffer;
};

class socket_pool_factory : public abstract_factory_of_socket_factory
{
public:
	std::unique_ptr<socket_factory> get_socket_factory( std::size_t size ) const override;
};

}
#endif //DOORMAT_SOCKET_POOL_H

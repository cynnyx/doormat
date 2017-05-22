#ifndef DOOR_MAT_MOCK_SERVER_H
#define DOOR_MAT_MOCK_SERVER_H

#include <cstdint>
#include <memory>
#include <type_traits>
#include "boost/asio.hpp"
#include "boost/asio/ssl.hpp"
#include "../../src/service_locator/service_locator.h"
#include "../src/io_service_pool.h"

/** \brief the mock server is used in order to simulate a board; it reads from a socket and responds on it
 * depending on the content that it received.*/
template<bool ssl = false>
class mock_server
{
	using tcp_socket_t = boost::asio::ip::tcp::socket;
	using ssl_socket_t = boost::asio::ssl::stream<tcp_socket_t>;
	using socket_t = std::conditional_t<ssl, ssl_socket_t, tcp_socket_t>;

	struct clear_tag {};
	struct ssl_tag {};

	uint16_t listening_port;
	std::unique_ptr<boost::asio::ip::tcp::acceptor> acceptor;
	std::unique_ptr<socket_t> socket;
	bool stopped{false};
	boost::asio::ssl::context ctx;

	std::unique_ptr<socket_t> make_socket(boost::asio::io_service&);

public:
	using read_feedback = std::function<void(std::string)>;
	using write_feedback = std::function<void(size_t)>;

	mock_server(uint16_t listening_port=8454);
	void start(std::function<void()> on_start_function, bool once = false);
	void stop();
	void read(int bytes, read_feedback rf = {});
	void write(const std::string& what, write_feedback wf = {});

	bool is_stopped() const noexcept;
};


#endif //DOOR_MAT_MOCK_SERVER_H

#ifndef DOOR_MAT_MOCK_SERVER_H
#define DOOR_MAT_MOCK_SERVER_H

#include <cstdint>
#include <memory>
#include <boost/asio.hpp>
#include "../../src/service_locator/service_locator.h"
#include "../src/io_service_pool.h"

/** \brief the mock server is used in order to simulate a board; it reads from a socket and responds on it
 * depending on the content that it received.*/
class mock_server
{
	uint16_t listening_port;
	std::unique_ptr<boost::asio::ip::tcp::acceptor> acceptor;
	std::unique_ptr<boost::asio::ip::tcp::socket> socket;
	bool stopped{false};

public:
	using read_feedback = std::function<void(std::string)>;
	using write_feedback = std::function<void(size_t)>;

	mock_server(uint16_t listening_port=8454) :
		listening_port{listening_port},
		stopped{false}
	{}

	void start(std::function<void()> on_start_function, bool once = false);
	void stop();
	void read(int bytes, read_feedback rf = {});
	void write(const std::string& what, write_feedback wf = {});

	bool is_stopped();
};


#endif //DOOR_MAT_MOCK_SERVER_H

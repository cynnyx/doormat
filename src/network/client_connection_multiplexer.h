#ifndef DOORMAT_CLIENT_CONNECTION_MULTIPLEXER_H
#define DOORMAT_CLIENT_CONNECTION_MULTIPLEXER_H

namespace network{

struct client_connection_handler
{
	client_connection_handler(std::size_t period = 1) : periodicity{periodicity}
	{}

	void timeout(std::shared_ptr<http::client_connection> conn){
		std::cout << "yes, timed out" << std::endl;
	}

	std::size_t periodicity;
};

class client_connection_multiplexer : public std::enable_shared_from_this<client_connection_multiplexer>
{
	std::shared_ptr<http::client_connection> connection;
	std::queue<std::weak_ptr<client_connection_handler>> handlers_list;

	std::size_t timeout;
	std::size_t self_periodicity{1};
	std::size_t max_periodicity{1};
	std::size_t tick_count{0};
public:
	client_connection_multiplexer(std::shared_ptr<http::client_connection> conn, std::size_t multiplexer_timeout) :
	connection{std::move(conn)}, timeout{multiplexer_timeout}
	{}

	std::size_t handlers_count() { return handlers_list.size(); }

	std::shared_ptr<client_connection_handler> get_handler();

	void update_timeout(std::chrono::milliseconds ms);

	void timeout();


	~client_connection_multiplexer() = default;
};

} //namespace network

#endif //DOORMAT_CLIENT_CONNECTION_MULTIPLEXER_H

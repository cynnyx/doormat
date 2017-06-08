#ifndef DOORMAT_CLIENT_CONNECTION_MULTIPLEXER_H
#define DOORMAT_CLIENT_CONNECTION_MULTIPLEXER_H

#include <functional>
#include <memory>
#include <list>
#include <chrono>
#include "../connection.h"
#include "response.h"
#include "request.h"

namespace http
{
class client_connection;
class connection_error;
}


namespace http{

class client_connection_multiplexer;

struct client_connection_handler
{
	friend class client_connection_multiplexer;
	client_connection_handler(std::shared_ptr<client_connection_multiplexer> multip) : multip{multip}, periodicity{0}
	{}

	void on_timeout(std::chrono::milliseconds ms, std::function<void(std::shared_ptr<http::connection>)> timeout_callback);
	void on_error(std::function<void(std::shared_ptr<http::connection>,  const http::connection_error &)> error_callback);

	std::pair<std::shared_ptr<http::client_request>, std::shared_ptr<http::client_response>> create_transaction();
private:
	void timeout(std::shared_ptr<http::connection> conn){ if(tcb) tcb(conn); }
	void error(std::shared_ptr<http::connection> conn, const http::connection_error &err) { if(ecb) ecb(conn, err); }

	std::function<void(std::shared_ptr<http::connection>,  const http::connection_error &)> ecb;

	std::function<void(std::shared_ptr<http::connection>)> tcb;

	std::shared_ptr<client_connection_multiplexer> multip;
	std::size_t periodicity;
};

class client_connection_multiplexer : public http::connection
{
	std::shared_ptr<http::client_connection> connection;
	std::list<std::weak_ptr<client_connection_handler>> handlers_list;

	std::size_t timeout_interval{0};
	std::size_t self_periodicity{1};
	std::size_t max_periodicity{1};
	std::size_t tick_count{0};

	static void deregister_handlers(std::shared_ptr<http::client_connection> conn);
	void clear_expired();
	void set_timeout(std::chrono::milliseconds ms) override {};

public:
	client_connection_multiplexer(std::shared_ptr<http::client_connection> conn) :
	connection{std::move(conn)}
	{}

	void init();
	void close() override;

	std::pair<std::shared_ptr<http::client_request>, std::shared_ptr<http::client_response>> create_transaction();

	std::size_t handlers_count() { return handlers_list.size(); }
	std::shared_ptr<client_connection_handler> get_handler();
	void update_timeout(std::chrono::milliseconds ms);
	void timeout();
	~client_connection_multiplexer() = default;
};

} //namespace network

#endif //DOORMAT_CLIENT_CONNECTION_MULTIPLEXER_H

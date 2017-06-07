#include "client_connection_multiplexer.h"
#include <cassert>
#include <numeric>
#include <algorithm>
#include "../http/client/client_connection.h"

namespace network
{

std::shared_ptr<client_connection_handler> client_connection_multiplexer::get_handler()
{
	auto newhandler = std::make_shared<client_connection_handler>(std::static_pointer_cast<client_connection_multiplexer>(this->shared_from_this()));
	handlers_list.push_back(newhandler);
	return newhandler;
}
template<typename t>
t GCD(t u, t v) {
	while ( v != 0) {
		t r = u % v;
		u = v;
		v = r;
	}
	return u;
}

void client_connection_handler::on_timeout(std::chrono::milliseconds ms, std::function<void(std::shared_ptr<http::connection>)> timeout_callback)
{
	tcb = std::move(timeout_callback);
	multip->update_timeout(std::chrono::milliseconds(ms.count()));
}


void client_connection_multiplexer::update_timeout(std::chrono::milliseconds ms)
{
	assert(ms.count() > 0);
	timeout_interval = (!timeout_interval) ? ms.count() : timeout_interval;
	std::size_t count = ms.count();
	assert(count);
	std::size_t old_gcd = timeout_interval;
	timeout_interval = GCD(count, timeout_interval);
	std::size_t multiplier = old_gcd / timeout_interval;
	self_periodicity *= multiplier;
	tick_count *= multiplier;
	max_periodicity = std::max(max_periodicity * multiplier, count / timeout_interval);

	std::for_each(handlers_list.begin(), handlers_list.end(), [multiplier, count, this](auto weak)
	{
		if(auto s = weak.lock())
		{
			if(!s->periodicity)
				s->periodicity = count/timeout_interval;
			else
				s->periodicity *= multiplier;
		}
	});

	connection->on_timeout(std::chrono::milliseconds{timeout_interval}, [this](auto conn)
	{
		this->timeout();
	});

}

void client_connection_multiplexer::timeout()
{
	//there was a timeout.
	++tick_count;
	std::for_each(handlers_list.begin(), handlers_list.end(), [this](auto weak){
		if(auto s = weak.lock()) {
			if(tick_count % s->periodicity == 0) {
				s->timeout(this->shared_from_this());
			}
		}
	});
	clear_expired();
}


void client_connection_multiplexer::clear_expired()
{
	auto b = handlers_list.begin();
	for(; b != handlers_list.end() && b->expired(); ++b)
		; //nothing
	handlers_list.erase(handlers_list.begin(), b); //erase all expired.
	if(handlers_list.empty()) deregister_handlers(connection);
}

void client_connection_multiplexer::init()
{

	//register and broadcast error.
	connection->on_error([this](auto conn, auto error){
		std::for_each(handlers_list.begin(), handlers_list.end(), [this, &error, &conn](auto weak)
		{
			if(auto h = weak.lock())
			{
				h->error(this->shared_from_this(), error);
			}
		});
		handlers_list.clear(); //after the error, we clear everything
		deregister_handlers(connection);
	});

	//not very sure about it! just remove whatever is expired, and everything should be fine.
	connection->on_request_sent([this](auto conn){
		this->clear_expired();
	});

}

void client_connection_multiplexer::close()
{
	http::connection_error err{http::error_code::connection_closed};
	std::for_each(handlers_list.begin(), handlers_list.end(), [this, &err](auto weak){
		if(auto h = weak.lock())
		{
			h->error(this->shared_from_this(), err);
		}
	});
	deregister_handlers(connection);
	connection->close();
}


void client_connection_multiplexer::deregister_handlers(std::shared_ptr<http::client_connection> conn)
{
	conn->on_error([](auto conn, auto error){});
	conn->on_request_sent([](auto conn){});
	conn->on_timeout(std::chrono::milliseconds{0}, [](auto conn){});

}

void client_connection_handler::on_error(std::function<void(std::shared_ptr<http::connection>,  const http::connection_error &)> error_callback)
{
	ecb = std::move(error_callback);
}

std::pair<std::shared_ptr<http::client_request>, std::shared_ptr<http::client_response>> client_connection_multiplexer::create_transaction() { return connection->create_transaction(); };

std::pair<std::shared_ptr<http::client_request>, std::shared_ptr<http::client_response>> client_connection_handler::create_transaction()
{
	return multip->create_transaction();
}



}
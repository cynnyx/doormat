#include "client_connection_multiplexer.h"


namespace network
{

std::shared_ptr<client_connection_handler> client_connection_multiplexer::get_handler()
{
	auto newhandler = std::make_shared<client_connection_handler>(1);
	handlers_list.push_back(newhandler);
	return newhandler;
}


void client_connection_multiplexer::update_timeout(std::chrono::milliseconds ms)
{

	std::size_t count = ms.count();
	assert(count);
	std::size_t old_gcd = timeout;
	timeout = std::gcd(count, timeout);
	std::size_t multiplier = old_gcd / timeout;
	self_periodicity *= multiplier;
	tick_count *= multiplier;
	max_periodicity = std::max(max_periodicity * multiplier, count / timeout);

	std::for_each(handlers_list.begin(), handlers_list.end(), [multiplier](auto weak){
		if(auto s = weak.lock()) s->periodicity *= multiplier;
	});

	connection->on_timeout(std::chrono::milliseconds{timeout}, [self = this->shared_from_this()](auto conn){
		self->timeout();
	});

}

void client_connection_multiplexer::timeout()
{
	//there was a timeout.
	++tick_count;
	std::for_each(handlers_list.begin(), handlers_list.end(), [](auto weak){
		if(auto s = weak.lock()) {
			if(tick_count % s->periodicity == 0) {
				s->timeout(connection);
			}
		}
	});
}



}
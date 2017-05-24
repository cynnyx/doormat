#ifndef DOORMAT_HANDLER_INTERFACE_H
#define DOORMAT_HANDLER_INTERFACE_H

#include <memory>
#include <boost/asio.hpp>
#include <iostream>
#include <experimental/optional>
#include "../http/http_commons.h"
#include "../http/server/server_connection.h"
#include "../utils/dstring.h"

class dstring;

/**
 * @note This "interface" violates all SOLID paradigm
 * static builder functions and unused methods - how to shoot yourself in the feet.
 *
 * At least three responsabilities found.
 */

namespace server
{
class connector_interface;

class http_handler
{
	std::weak_ptr<connector_interface> _connector;

	virtual void do_write() = 0;
	virtual void on_connector_nulled() = 0;

protected:


	std::shared_ptr<connector_interface> connector()
	{
		return _connector.lock();
    }
	std::shared_ptr<const connector_interface> connector() const
    {
		return _connector.lock();
	}


public:
	http_handler() = default;
	//void close() override;
	void connector( std::shared_ptr<connector_interface> conn);
	boost::asio::ip::address find_origin() const;

	virtual bool start() noexcept = 0;
	virtual bool should_stop() const noexcept = 0;
	virtual bool on_read(const char*, unsigned long) = 0;
	virtual bool on_write(std::string& chunk) = 0;
	virtual void trigger_timeout_event() =0;
	virtual std::vector<std::pair<std::function<void()>, std::function<void()>>> write_feedbacks(){ return {}; }

	virtual ~http_handler() = default;
};

}

#endif //DOORMAT_HANDLER_INTERFACE_H

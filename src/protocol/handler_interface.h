#include <memory>
#include <boost/asio.hpp>
#include <iostream>
#include <experimental/optional>
#include "../http/http_commons.h"
#include "../http/connection.h"
#ifndef DOORMAT_HANDLER_INTERFACE_H
#define DOORMAT_HANDLER_INTERFACE_H

/**
 * @note This "interface" violates all SOLID paradigm
 * static builder functions and unused methods - how to shoot yourself in the feet.
 *
 * At least three responsabilities found.
 */

namespace server
{
class connector_interface;
class handler_interface : public http::connection
{
	std::weak_ptr<connector_interface> _connector;
protected:
	virtual void do_write() = 0;
	virtual void on_connector_nulled() = 0;
	std::shared_ptr<handler_interface> get_shared()
	{
		return std::static_pointer_cast<handler_interface>(this->shared_from_this());
	}

	std::shared_ptr<connector_interface> connector() noexcept
	{
		if(auto s = _connector.lock()) return s;
		return nullptr;
	}
	const std::shared_ptr<connector_interface> connector() const noexcept
	{
		if(auto s = _connector.lock()) return s;
		return nullptr;
	}


public:
	handler_interface() = default;
	void close() override;
    void connector( std::shared_ptr<connector_interface> conn);


	void notify_write() noexcept { do_write(); }
	boost::asio::ip::address find_origin() const;
	virtual bool start() noexcept = 0;
	virtual bool should_stop() const noexcept = 0;
	virtual bool on_read(const char*, unsigned long) = 0;
	virtual bool on_write(dstring& chunk) = 0;

	virtual void on_error(const int &) = 0;

    virtual ~handler_interface() = default;
};

}

#endif //DOORMAT_HANDLER_INTERFACE_H

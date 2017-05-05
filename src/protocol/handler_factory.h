#pragma once

#include <memory>
#include <functional>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include "../../deps/openssl/include/openssl/ssl.h"

#include "../http/http_commons.h"

class dstring;
struct node_interface;

namespace http
{
	class http_response;
}

namespace server
{
using interval = boost::posix_time::time_duration;

constexpr const size_t MAXINBYTESPERLOOP{8192};

enum handler_type
{
	ht_unknown,
	ht_h1,
	ht_h2
};

class connector_interface;
using tcp_socket = boost::asio::ip::tcp::socket;
using ssl_socket = boost::asio::ssl::stream<tcp_socket>;


/**
 * @note This "interface" violates all SOLID paradigm
 * static builder functions and unused methods - how to shoot yourself in the feet.
 *
 * At least three responsabilities found.
 */
class handler_interface : public std::enable_shared_from_this<handler_interface>
{
    std::weak_ptr<connector_interface> _connector;
protected:
	virtual void do_write() = 0;
	virtual void on_connector_nulled() = 0;
public :
	handler_interface() = default;
	virtual ~handler_interface() = default;

	void initialize_callbacks(node_interface& cor);

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
	void connector( std::shared_ptr<connector_interface> conn);
	void notify_write() noexcept { do_write(); }
	boost::asio::ip::address find_origin() const;

	virtual bool start() noexcept = 0;
	virtual bool should_stop() const noexcept = 0;
	virtual bool on_read(const char*, size_t) = 0;
	virtual bool on_write(dstring& chunk) = 0;

	virtual void on_eom() = 0;
	virtual void on_error(const int &) = 0;
};

class handler_factory
{
    std::shared_ptr<handler_interface> make_handler(handler_type, http::proto_version) const noexcept;

public:
	void register_protocol_selection_callbacks(SSL_CTX* ctx);
	std::shared_ptr<handler_interface> negotiate_handler(std::shared_ptr<ssl_socket> s,interval, interval) const noexcept;
    std::shared_ptr<handler_interface> build_handler(handler_type, http::proto_version vers, interval, interval, std::shared_ptr<ssl_socket> s) const noexcept;
	std::shared_ptr<handler_interface> build_handler(handler_type, http::proto_version vers, interval, interval, std::shared_ptr<tcp_socket> socket) const noexcept;
};

} //namespace

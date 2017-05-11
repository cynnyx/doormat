#pragma once

#include <memory>
#include <functional>
#include <boost/asio.hpp>
#include "openssl/ssl.h"

#include "../http/http_commons.h"

class dstring;
struct node_interface;

namespace http
{
	class http_response;
}

namespace server
{

constexpr const size_t MAXINBYTESPERLOOP{8192};

enum handler_type
{
	ht_unknown,
	ht_h1,
	ht_h2
};

class connector_interface;

/**
 * @note This "interface" violates all SOLID paradigm
 * static builder functions and unused methods - how to shoot yourself in the feet.
 *
 * At least three responsabilities found.
 */
class handler_interface
{
	connector_interface* _connector{nullptr};
protected:
	virtual void do_write() = 0;
	virtual void on_connector_nulled() = 0;
public :
	handler_interface() = default;
	virtual ~handler_interface() = default;

	void initialize_callbacks(node_interface& cor);

	connector_interface* connector() noexcept { return _connector; }
	const connector_interface* connector() const noexcept { return _connector; }
	void connector( connector_interface * conn);
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
public:
	void register_protocol_selection_callbacks(SSL_CTX* ctx);
	handler_interface* negotiate_handler(const SSL* ssl) const noexcept;
	handler_interface* build_handler(handler_type, http::proto_version vers = http::proto_version::UNSET) const noexcept;
};

} //namespace

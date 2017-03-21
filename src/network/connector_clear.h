#ifndef CONNECTOR_CLEAR_H
#define CONNECTOR_CLEAR_H

#include <memory>
#include "connector.h"
#include "buffer.h"
#include "../http/http_codec.h"

namespace network
{

class connector_clear : public connector, public std::enable_shared_from_this<connector_clear>
{
	using socket = boost::asio::ip::tcp::socket;
	http::http_codec codec;
	buffer<boost::asio::ip::tcp::socket> output;
	bool close_on_eom{false};
public:
	connector_clear( const http::http_request &req, receiver& recv );
	void send_body( dstring&& body ) noexcept override;
	void send_trailer( dstring&&, dstring&&) noexcept override;
	void send_eom() noexcept override;
	void on_header(http::http_response &&) noexcept override;
	void on_body(dstring&&) noexcept override;
	void on_trailer(dstring&&, dstring&&) noexcept override;
	void on_eom() noexcept override;
	void stop() noexcept override;
	void on_error( int error ) noexcept override;
};

}
#endif // CONNECTOR_CLEAR_H

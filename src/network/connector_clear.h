#ifndef CONNECTOR_CLEAR_H
#define CONNECTOR_CLEAR_H

#include <memory>
#include "connector.h"
#include "socket_factory.h"
#include "buffer.h"
#include "../http/http_codec.h"

namespace network
{

class connector_clear : public connector, public std::enable_shared_from_this<connector_clear>
{
	using socket = boost::asio::ip::tcp::socket;
	http::http_codec codec;
	buffer<socket> output;
	http::http_response input;
	std::unique_ptr<network::socket_factory<socket>> socket_ref;
	errors::error_code errcode;
	bool close_on_eom{false};
	bool http_continue{false};
	connector_clear( const http::http_request &reqx, std::shared_ptr<receiver>& recv );
	void init() noexcept;
public:
	static std::shared_ptr<connector_clear> make_connector_clear( const http::http_request &req, 
		std::shared_ptr<receiver>& recv );
	void send_body( dstring&& body ) noexcept override;
	void send_trailer( dstring&&, dstring&&) noexcept override;
	void send_eom() noexcept override;
	void on_header(http::http_response &&) noexcept override;
	void on_body(dstring&&) noexcept override;
	void on_trailer(dstring&&, dstring&&) noexcept override;
	void on_eom() noexcept override;
	void stop() noexcept override;
	void on_error( int error ) noexcept override;
	void on_response_continue() noexcept override;
};

}
#endif // CONNECTOR_CLEAR_H

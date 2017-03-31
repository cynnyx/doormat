#ifndef CONNECTOR_SSL_H
#define CONNECTOR_SSL_H

#include "connector.h"

namespace network
{
	
class connector_ssl : public connector
{
public:
	connector_ssl( const http::http_request &req, std::shared_ptr<receiver>& rec ): connector{req, rec} {}
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
#endif // CONNECTOR_SSL_H

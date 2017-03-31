#ifndef DOORMAT_CONNECTOR_H
#define DOORMAT_CONNECTOR_H

#include <memory>
#include "../utils/dstring.h"
#include "../http/http_request.h"
#include "receiver.h"

namespace network
{
	
class connector 
{
protected:
	const http::http_request & req_ref;
	std::shared_ptr<receiver> rec;
public:
	connector( const http::http_request &req, std::shared_ptr<receiver>& rec_ ) noexcept: req_ref{req}, rec{rec_} {}
	virtual void send_body( dstring&& body ) noexcept = 0;
	virtual void send_trailer( dstring&&, dstring&&) noexcept = 0;
	virtual void send_eom() noexcept = 0;
	virtual void on_header(http::http_response &&) noexcept = 0;
	virtual void on_body(dstring&&) noexcept = 0;
	virtual void on_trailer(dstring&&, dstring&&) noexcept = 0;
	virtual void on_eom() noexcept = 0;
	virtual void stop() noexcept = 0;
	virtual void on_error( int error ) noexcept = 0;
	virtual void on_response_continue() noexcept = 0;
	virtual ~connector() = default;
};

std::unique_ptr<connector> create_connector( const http::http_request &req, receiver& rec );

}

#endif //DOORMAT_COMMUNICATOR_H

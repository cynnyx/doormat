#ifndef DOORMAT_CONNECTOR_H
#define DOORMAT_CONNECTOR_H

#include <memory>
#include "../http/http_request.h"
#include "../http/http_response.h"

namespace network
{
	
/**
 * @class receiver
 * 
 * This interface is to be implemented for those who want to receive connector's events.
 */
class receiver
{
public:
	virtual ~receiver() = default;
	virtual void on_header(http::http_response &&) noexcept = 0;
	virtual void on_body(dstring&&) noexcept = 0;
	virtual void on_trailer(dstring&&, dstring&&) noexcept = 0;
	virtual void on_eom() noexcept = 0;
	virtual void on_error( int error ) noexcept = 0;
	virtual void stop() noexcept = 0;
};
	
class connector 
{
protected:
	const http::http_request & req_ref;
	receiver& rec;
public:
	connector( const http::http_request &req, receiver& rec_ ) noexcept: req_ref{req}, rec(rec_) {}
	virtual void send_body( dstring&& body ) noexcept = 0;
	virtual void send_trailer( dstring&&, dstring&&) noexcept = 0;
	virtual void send_eom() noexcept = 0;
	virtual void on_header(http::http_response &&) noexcept = 0;
	virtual void on_body(dstring&&) noexcept = 0;
	virtual void on_trailer(dstring&&, dstring&&) noexcept = 0;
	virtual void on_eom() noexcept = 0;
	virtual void stop() noexcept = 0;
	virtual void on_error( int error ) noexcept = 0;
	virtual ~connector() = default;
};

std::unique_ptr<connector> create_connector( const http::http_request &req, receiver& rec );

}

#endif //DOORMAT_COMMUNICATOR_H

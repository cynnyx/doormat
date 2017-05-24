#include "request.h"
#include "../server/server_connection.h"

namespace http {

/** Ctor */
request::request(std::shared_ptr<server_connection> conn) : connection_keepalive{std::move(conn)}
{}

/** Events registration. */

void request::on_headers(headers_callback_t hcb)
{
	headers_callback = std::move(hcb);
}

void request::on_body(body_callback_t bcb)
{
	body_callback = std::move(bcb);
}

void request::on_trailer(trailer_callback_t tcb)
{
	trailer_callback = std::move(tcb);
}

void request::on_finished(finished_callback_t fcb)
{
	finished_callback = std::move(fcb);
}

void request::on_error(error_callback_t ecb)
{
	error_callback = std::move(ecb);
}

/** Events triggers */
void request::headers(http::http_request &&req)
{
	_preamble = std::move(req);
	if(headers_callback)
		headers_callback(this->shared_from_this());
}

void request::body(data_t d, size_t s)
{
	if(body_callback)
		body_callback(this->shared_from_this(), std::move(d), s);
}

void request::trailer(std::string&& k, std::string&& v)
{
	if(trailer_callback)
		trailer_callback(this->shared_from_this(), std::string(k), std::string(v));
}

void request::finished()
{
	if(request_terminated) return;
	if(finished_callback)
		finished_callback(this->shared_from_this());
	request_terminated = true;
}

void request::error(http::connection_error err)
{
	if(request_terminated) return;
	conn_error = std::move(err);
	if(error_callback)
		error_callback(this->shared_from_this(), conn_error);
	request_terminated = true;
}

std::shared_ptr<server_connection> request::get_connection()
{
	return connection_keepalive;
}

}

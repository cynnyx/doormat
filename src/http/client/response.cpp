#include "response.h"
#include "../client/client_connection.h"

namespace http {

/** Ctor */
client_response::client_response(std::shared_ptr<client_connection> conn) : connection_keepalive{std::move(conn)}
{}

/** Events registration. */

void client_response::on_headers(headers_callback_t hcb)
{
	headers_callback = std::move(hcb);
}

void client_response::on_body(body_callback_t bcb)
{
	body_callback = std::move(bcb);
}

void client_response::on_trailer(trailer_callback_t tcb)
{
	trailer_callback = std::move(tcb);
}

void client_response::on_finished(finished_callback_t fcb)
{
	finished_callback = std::move(fcb);
}

void client_response::on_response_continue(continue_callback_t ccb)
{
	continue_callback = std::move(ccb);
}

void client_response::on_error(error_callback_t ecb)
{
	error_callback = std::move(ecb);
}

/** Events triggers */
void client_response::headers(http::http_response &&res)
{
	_preamble = std::move(res);
	if(headers_callback)
		headers_callback(this->shared_from_this());
}

void client_response::body(data_t d, size_t s)
{
	if(body_callback)
		body_callback(this->shared_from_this(), std::move(d), s);
}

void client_response::trailer(std::string&& k, std::string&& v)
{
	if(trailer_callback)
		trailer_callback(this->shared_from_this(), std::string(k), std::string(v));
}

void client_response::finished()
{
	if(response_ended) return;
	if(finished_callback)
		finished_callback(this->shared_from_this());
	response_ended = true;
}

void client_response::response_continue()
{
	if(response_ended) return;
	if(continue_callback)
		continue_callback(this->shared_from_this());
}

void client_response::error(http::connection_error err)
{
	if(response_ended) return;
	conn_error = std::move(err);
	if(error_callback)
		error_callback(this->shared_from_this(), conn_error);
	response_ended = true;
}

std::shared_ptr<client_connection> client_response::get_connection()
{
	return connection_keepalive;
}

}

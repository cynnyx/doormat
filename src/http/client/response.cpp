#include "response.h"
#include "../client/client_connection.h"

namespace http {

/** Ctor */
client_response::client_response(std::shared_ptr<client_connection> conn) : connection_keepalive{std::move(conn)}
{}

/** Events registration. */

void client_response::on_headers(headers_callback_t hcb)
{
	headers_callback.emplace(std::move(hcb));
}

void client_response::on_body(body_callback_t bcb)
{
	body_callback.emplace(std::move(bcb));
}

void client_response::on_trailer(trailer_callback_t tcb)
{
	trailer_callback.emplace(std::move(tcb));
}

void client_response::on_finished(finished_callback_t fcb)
{
	finished_callback.emplace(std::move(fcb));
}

void client_response::on_error(error_callback_t ecb)
{
	error_callback.emplace(std::move(ecb));
}

/** Events triggers */
void client_response::headers(http::http_response &&res)
{
	_preamble = std::move(res);
	if(headers_callback) (*headers_callback)(this->shared_from_this());
}

void client_response::body(dstring&& body)
{
	//todo: avoid copying
    std::unique_ptr<char[]> b{ new char[body.size()]};
	std::memcpy(b.get(), body.cdata(), body.size());
	if(body_callback) (*body_callback)(this->shared_from_this(), std::move(b), body.size());
}

void client_response::trailer(dstring &&k, dstring &&v)
{
	if(trailer_callback) (*trailer_callback)(this->shared_from_this(), std::string(k), std::string(v));
}

void client_response::finished()
{
	if(response_ended) return;
	if(finished_callback) (*finished_callback)(this->shared_from_this());
	response_ended = true;
}

void client_response::error(http::connection_error err)
{
	if(response_ended) return;
	conn_error = std::move(err);
	if(error_callback) (*error_callback)(this->shared_from_this(), conn_error);
	response_ended = true;
}

std::shared_ptr<client_connection> client_response::get_connection()
{
	return connection_keepalive;
}

}

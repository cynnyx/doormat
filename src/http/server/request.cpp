#include "request.h"
#include "../server/server_connection.h"

namespace http {

/** Ctor */
request::request(std::shared_ptr<server_connection> conn) : connection_keepalive{std::move(conn)}
{}

/** Events registration. */

void request::on_headers(headers_callback_t hcb)
{
	headers_callback.emplace(std::move(hcb));
}

void request::on_body(body_callback_t bcb)
{
    body_callback.emplace(std::move(bcb));
}

void request::on_trailer(trailer_callback_t tcb)
{
    trailer_callback.emplace(std::move(tcb));
}

void request::on_finished(finished_callback_t fcb)
{
    finished_callback.emplace(std::move(fcb));
}

void request::on_error(error_callback_t ecb)
{
    error_callback.emplace(std::move(ecb));
}

/** Events triggers */
void request::headers(http::http_request &&req)
{
	_preamble = std::move(req);
    if(headers_callback) (*headers_callback)(this->shared_from_this());
}

void request::body(dstring&& body)
{
	//todo: avoid copying
	std::unique_ptr<char> b{ new char[body.size()]};
	std::memcpy(b.get(), body.cdata(), body.size());
    if(body_callback) (*body_callback)(this->shared_from_this(), std::move(b), body.size());
}

void request::trailer(dstring &&k, dstring &&v)
{
    if(trailer_callback) (*trailer_callback)(this->shared_from_this(), std::string(k), std::string(v));
}

void request::finished()
{
	if(!myself) return;
	if(finished_callback) (*finished_callback)(this->shared_from_this());
	myself = nullptr;
}

void request::error(http::connection_error err)
{
	if(!myself) return;
	conn_error = std::move(err);
	if(error_callback) (*error_callback)(this->shared_from_this(), conn_error);
	myself=nullptr;
}

std::shared_ptr<server_connection> request::get_connection()
{
	return connection_keepalive;
}

}

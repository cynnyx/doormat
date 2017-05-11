#include "request.h"

namespace http {

/** Ctor */
request::request(std::shared_ptr<connection> conn) : connection_keepalive{std::move(conn)}
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
    if(headers_callback) (*headers_callback)(*this, std::move(req));
}

void request::body(dstring&& body)
{
    if(body_callback) (*body_callback)(*this, std::move(body));
}

void request::trailer(dstring &&k, dstring &&v)
{
    if(trailer_callback) (*trailer_callback)(*this, std::move(k), std::move(v));
}

void request::finished()
{
    if(finished_callback) (*finished_callback)(*this);
    myself = nullptr;
}

}
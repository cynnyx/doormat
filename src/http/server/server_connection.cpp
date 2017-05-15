#include "server_connection.h"

namespace http {

void server_connection::on_request(request_callback rcb)
{
	request_cb = std::move(rcb);
}

void server_connection::request_received(std::shared_ptr<http::request> req, std::shared_ptr<http::response> res)
{
	if(request_cb) request_cb(std::static_pointer_cast<server_connection>(this->shared_from_this()), req, res);
}

}

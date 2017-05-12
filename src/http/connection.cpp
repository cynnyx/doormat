#include "connection.h"
#include "request.h"
#include "response.h"

namespace http
{

void connection::request_received(std::shared_ptr<http::request> req, std::shared_ptr<http::response> res)
{
	if(request_cb) request_cb(this->shared_from_this(), req, res);
}

void connection::error(http::connection_error error)
{
	current = error;
	if(error_cb) (*error_cb)(this->shared_from_this(), current);
}

void connection::on_error(error_callback ecb)
{
	error_cb.emplace(std::move(ecb));
}

void connection::on_request(request_callback rcb)
{
	request_cb = std::move(rcb);
}

}

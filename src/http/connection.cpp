#include "connection.h"
#include "server/request.h"
#include "server/response.h"

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


void connection::on_timeout(std::chrono::milliseconds ms, timeout_callback tcb)
{
	timeout_cb.emplace(std::move(tcb));
	set_timeout(ms);
}

}

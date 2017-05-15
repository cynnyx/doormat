#include "connection.h"
#include "server/request.h"
#include "server/response.h"

namespace http
{

void connection::error(http::connection_error error)
{
	current = error;
	if(error_cb) (*error_cb)(this->shared_from_this(), current);
}

void connection::on_error(error_callback ecb)
{
	error_cb.emplace(std::move(ecb));
}

void connection::on_timeout(std::chrono::milliseconds ms, timeout_callback tcb)
{
	timeout_cb.emplace(std::move(tcb));
	set_timeout(ms);
}

}

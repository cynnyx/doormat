#include "client_connection.h"

namespace http {

void client_connection::on_response(response_callback rcb)
{
	response_cb = std::move(rcb);
}

void client_connection::response_received(std::shared_ptr<http::client_response> res)
{
	if(response_cb) response_cb(std::static_pointer_cast<client_connection>(this->shared_from_this()), std::move(res));
}

}

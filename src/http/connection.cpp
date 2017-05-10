#include "connection.h"
#include "request.h"
#include "response.h"

namespace http
{

void connection::request_received(std::shared_ptr<http::request> req, std::shared_ptr<http::response> res)
{
	if(request_cb) request_cb(req, res);

	return;
}

}

#include "connection.h"
#include "request.h"
#include "response.h"

#include <iostream>
namespace http
{
void connection::request_received(std::shared_ptr<http::request> req, std::shared_ptr<http::response> res)
{
	if(request_cb) request_cb(req, res);

	return;
}


std::tuple<std::function<void()>, std::function<void(dstring&&)>, std::function<void(dstring&&, dstring&&)>, std::function<void()>> connection::get_request_handlers()
{
	auto hcb = [this](){ std::cout << "header from http2" << std::endl; };
	auto bcb = [this](dstring&&b){/*notify_body(std::move(b));*/};
	auto tcb = [this](dstring&&b, dstring&&c){/*notify_trailers(std::move(b), std::move(c));*/};
	auto ccb = [this](){/*notify_finished();*/};
	return std::make_tuple(hcb, bcb, tcb, ccb);
}

}

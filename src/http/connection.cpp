#include "connection.h"
#include "request.h"
#include "response.h"

#include <iostream>
namespace http
{
http_request* connection::request_received()
{
	auto req_handler = std::make_shared<http::request>(this->shared_from_this());
	auto res_handler = std::make_shared<http::response>([s = this->shared_from_this()](){
		s->notify_response();
	});
	if(request_cb) request_cb(req_handler, res_handler);
	requests.emplace(std::move(req_handler));
	responses.push(std::move(res_handler));
	return &current_request;
}


http_request* connection::request_received(std::function<void(http_response&&)> hcb, std::function<void(dstring&&)> bcb, std::function<void(dstring&&, dstring&&)> tcb, std::function<void()> ccb)
{
	auto req_handler = std::make_shared<http::request>(this->shared_from_this());
	auto res_handler = std::make_shared<http::response>(hcb, bcb, tcb, ccb);

	if(request_cb) request_cb(req_handler, res_handler);
	requests.emplace(std::move(req_handler));
	responses.push(std::move(res_handler));

	return &current_request;
}


std::tuple<std::function<void()>, std::function<void(dstring&&)>, std::function<void(dstring&&, dstring&&)>, std::function<void()>> connection::get_request_handlers()
{
	auto hcb = [this](){ notify_headers();};
	auto bcb = [this](dstring&&b){notify_body(std::move(b));};
	auto tcb = [this](dstring&&b, dstring&&c){notify_trailers(std::move(b), std::move(c));};
	auto ccb = [this](){notify_finished(); };
	return std::make_tuple(hcb, bcb, tcb, ccb);
}


/** \brief method called once current_request contains the http preamble and all the headers readed from the socket.
 * */
void connection::notify_headers()
{
	auto &last_req = requests.back();
	if(auto s = last_req.lock()) //notify data to alive requests.
	{
		s->headers(std::move(current_request));
	}
	/** re-assign current_request so that it is valid again*/
	current_request = http_request{};
}

/** \brief method called every time a portion of the body has been readed.
 *
 * */
void connection::notify_body(dstring&& body)
{
	auto &last_req = requests.back();
	if(auto s = last_req.lock()) //notify data to alive requests.
	{
		s->body(std::move(body));
	}
}

void connection::notify_finished()
{
	auto &last_req = requests.back();
	if(auto s = last_req.lock()) //notify data to alive requests.
	{
		s->finished();
	}
}

void connection::notify_trailers(dstring &&k, dstring &&v)
{
	auto &last_req = requests.back();
	if(auto s = last_req.lock()) //notify data to alive requests.
	{
		s->trailer(std::move(k), std::move(v));
	}
}

void connection::notify_error(bool global)
{
	if(global) {
		while(!requests.empty())
		{
			if(auto s = requests.front().lock()) s->error();
			requests.pop();
		}
	}
}

bool connection::poll_response(std::shared_ptr<response> res) {
	auto state = res->get_state();
	while(state != http::response::state::pending) {
		switch(state) {
		case http::response::state::headers_received:
			notify_response_headers(res->get_headers()); break;
		case http::response::state::body_received:
			notify_response_body(res->get_body()); break;
		case http::response::state::trailer_received:
		{
			auto trailer = res->get_trailer();
			notify_response_trailer(std::move(trailer.first), std::move(trailer.second));
			break;
		}
		case http::response::state::ended:
			notify_response_end();
			return true;
		default: assert(0);
		}
		state = res->get_state();
	}
	return false;
}

void connection::notify_response()
{
	while(!responses.empty())
	{
		//replace all this with a polling mechanism!
		auto &c = responses.front();
		if(auto s = c.lock()) {
			if(poll_response(s)) responses.pop();
			else break;
		}
	}
}

}

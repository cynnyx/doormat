#include "request.h"
#include <functional>
#include <memory>
#include <iostream>

namespace http
{

client_request::client_request(std::function<void()> content_notification,boost::asio::io_service&io) : io{io}
{
	hcb = [this, content_notification](http_request &&res){
		request_headers.emplace(std::move(res));
		content_notification();
	};

	bcb = [this, content_notification](data_t d, size_t s) {
		content.append(d.get(), s);
		content_notification();
	};

	tcb =[this, content_notification](std::string&& k, std::string&& v) {
		trailers.emplace(std::make_pair(std::move(k), std::move(v)));
		content_notification();
	};

	ccb = [this, content_notification](){
		ended = true;
		content_notification();
	};
}

client_request::client_request(std::function<void(http_request&&)> hcb, std::function<void(data_t, size_t)> bcb, std::function<void(std::string&&, std::string&&)> tcb,
				   std::function<void()> ccb,boost::asio::io_service&io) :
	hcb{std::move(hcb)}, bcb{std::move(bcb)}, tcb{std::move(tcb)}, ccb{std::move(ccb)}, io{io}
{}


void client_request::headers(http_request &&res) { hcb(std::move(res));  }
void client_request::body(data_t d, size_t s){ bcb(std::move(d), s);  }
void client_request::trailer(std::string&& k, std::string&& v) { tcb(std::move(k), std::move(v)); }
void client_request::end() {  ccb();  myself= this->shared_from_this();  }

void client_request::on_error(error_callback_t ecb) { error_callback = std::move(ecb); }
void client_request::on_write(write_callback_t wcb) { write_callback = std::move(wcb); }
client_request::state client_request::get_state() const noexcept
{
	if(bool(request_headers))
	{
		return state::headers_received;
	}
	if(content.size()) return state::body_received;
	if(trailers.size()) return state::trailer_received;
	if(ended) return state::ended;
	return state::pending;
}

http_request client_request::preamble()
{
	auto empty_response = std::move(*request_headers);
	request_headers = std::experimental::nullopt;
	assert(!request_headers);
	return empty_response;
}

std::string client_request::get_body() {
	std::string ret;
	std::swap(ret, content);
	return ret;
}

std::pair<std::string, std::string> client_request::get_trailer() {
	auto trailer = trailers.front();
	trailers.pop();
	return trailer;
}

}

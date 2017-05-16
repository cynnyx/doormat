#include "request.h"
#include <functional>
#include <memory>

namespace http
{

client_request::client_request(std::function<void()> content_notification)
{
	hcb = [this, content_notification](http_request &&res){
		request_headers.emplace(std::move(res));
		content_notification();
	};

	bcb = [this, content_notification](dstring &&d) {
		content.append(d);
		content_notification();
	};

	tcb =[this, content_notification](dstring &&k, dstring&&v) {
		trailers.emplace(std::make_pair(std::move(k), std::move(v)));
		content_notification();
	};

	ccb = [this, content_notification](){
		ended = true;
		content_notification();
	};
}

client_request::client_request(std::function<void(http_request&&)> hcb, std::function<void(dstring &&)> bcb, std::function<void(dstring &&, dstring &&)> tcb,
				   std::function<void()> ccb) :
	hcb{std::move(hcb)}, bcb{std::move(bcb)}, tcb{std::move(tcb)}, ccb{std::move(ccb)}
{}


void client_request::headers(http_request &&res) { hcb(std::move(res));  }
void client_request::body(dstring &&d){ bcb(std::move(d));  }
void client_request::trailer(dstring &&k, dstring&& v) { tcb(std::move(k), std::move(v)); }
void client_request::end() { ccb(); }

void client_request::on_error(error_callback_t ecb) { error_callback.emplace(std::move(ecb)); }

client_request::state client_request::get_state() const noexcept
{
	if(bool(request_headers)) {
		return state::headers_received;
	}
	if(content.size()) return state::body_received;
	if(trailers.size()) return state::trailer_received;
	if(ended) return state::ended;
	return state::pending;
}

http_request client_request::get_preamble()
{
	auto empty_response = std::move(*request_headers);
	request_headers = std::experimental::nullopt;
	assert(!request_headers);
	return empty_response;
}

dstring client_request::get_body() {
	dstring tmp = content;
	content = dstring{};
	return tmp;
}

std::pair<dstring, dstring> client_request::get_trailer() {
	auto trailer = trailers.front();
	trailers.pop();
	return trailer;
}

}

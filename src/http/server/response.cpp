#include "response.h"
#include <functional>
#include <memory>

namespace http
{

response::response(std::function<void()> content_notification, boost::asio::io_service&io ) : io{io}
{
	hcb = [this, content_notification](http_response&& res){
		response_headers.emplace(std::move(res));
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

	notify_continue = content_notification;
}

response::response(std::function<void(http_response&&)> hcb, std::function<void(data_t, size_t)> bcb, std::function<void(std::string&&, std::string&&)> tcb,
				   std::function<void()> ccb, boost::asio::io_service&io ) :
    hcb{std::move(hcb)}, bcb{std::move(bcb)}, tcb{std::move(tcb)}, ccb{std::move(ccb)}, io{io}
{}


void response::headers(http_response &&res) { hcb(std::move(res));  }
void response::body(data_t d, size_t s){ bcb(std::move(d), s);  }
void response::trailer(std::string&& k, std::string&& v) { tcb(std::move(k), std::move(v)); }
void response::end() { myself = this->shared_from_this(); ccb();}

void response::on_error(error_callback_t ecb) { error_callback = std::move(ecb); }
void response::on_write(write_callback_t wcb) { write_callback = std::move(wcb); }

enum class state {
	pending,
	headers_received,
	body_received,
	trailer_received,
	ended
} current = state::pending;

response::state response::get_state() noexcept
{
	if(continue_required)
	{
		continue_required = false;
		return state::send_continue;
	}
	if(bool(response_headers)) {
		return state::headers_received;
	}
	if(content.size()) return state::body_received;
	if(trailers.size()) return state::trailer_received;
	if(ended) return state::ended;
	return state::pending;
}

http_response response::get_preamble()
{
	auto empty_response = std::move(*response_headers);
	response_headers = std::experimental::nullopt;
	assert(!response_headers);
	return empty_response;
}

std::string response::get_body() {
	std::string ret;
	std::swap(content, ret);
	return ret;
}

std::pair<std::string, std::string> response::get_trailer() {
	auto trailer = trailers.front();
	trailers.pop();
	return trailer;
}

}

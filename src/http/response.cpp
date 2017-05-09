#include "response.h"
#include <functional>
#include <memory>

namespace http
{

response::response(std::function<void()> content_notification)
{
    hcb = [this, content_notification](http_response &&res){
        response_headers.emplace(std::move(res));
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

response::response(std::function<void(http_response&&)> hcb, std::function<void(dstring &&)> bcb, std::function<void(dstring &&, dstring &&)> tcb,
                   std::function<void()> ccb) :
hcb{std::move(hcb)}, bcb{std::move(bcb)}, tcb{std::move(tcb)}, ccb{std::move(ccb)}
{}


void response::headers(http_response &&res) { hcb(std::move(res));  }
void response::body(dstring &&d){ bcb(std::move(d));  };
void response::trailer(dstring &&k, dstring&& v) { tcb(std::move(k), std::move(v)); }
void response::end() { ccb(); }

void response::on_error(error_callback_t ecb) { error_callback.emplace(std::move(ecb)); }


enum class state {
    pending,
    headers_received,
    body_received,
    trailer_received,
    ended
} current = state::pending;

response::state response::get_state()
{
    if(bool(response_headers)) {
        return state::headers_received;
    }
    if(content.size()) return state::body_received;
    if(trailers.size()) return state::trailer_received;
    if(ended) return state::ended;
    return state::pending;
}

http_response response::get_headers()
{
    auto empty_response = std::move(*response_headers);
    response_headers = std::experimental::nullopt;
    assert(!response_headers);
    return empty_response;
}

dstring response::get_body() {
    dstring tmp = content;
    content = dstring{};
    return tmp;
}

std::pair<dstring, dstring> response::get_trailer() {
    auto trailer = trailers.front();
    trailers.pop();
    return trailer;
};

}
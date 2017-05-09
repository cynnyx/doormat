#ifndef DOORMAT_RESPONSE_H
#define DOORMAT_RESPONSE_H

#include <iostream>
#include "http_response.h"

namespace http
{

class connection;

class response
{
    friend connection;
public:
    using error_callback_t = std::function<void()>;
    //for now it will write back immediately!
    response(std::function<void()> content_notification) : content_notification{std::move(content_notification)}
    {}
    void headers(http_response &&res) { response_headers.emplace(std::move(res)); content_notification();}
    void body(dstring &&d){ content.append(d); content_notification();};
    void trailer(dstring &&k, dstring&& v) { trailers.emplace(std::make_pair(std::move(k), std::move(v)));content_notification(); }
    void end() { ended = true;  content_notification(); }
    void on_error(error_callback_t ecb) { error_callback.emplace(std::move(ecb)); }


    enum class state {
        pending,
        headers_received,
        body_received,
        trailer_received,
        ended
    } current = state::pending;

    state get_state()
    {
        if(bool(response_headers)) {
            return state::headers_received;
        }
        if(content.size()) return state::body_received;
        if(trailers.size()) return state::trailer_received;
        if(ended) return state::ended;
        return state::pending;
    }

    http_response get_headers()
    {
        auto empty_response = std::move(*response_headers);
        response_headers = std::experimental::nullopt;
        assert(!response_headers);
        return empty_response;
    }

    dstring get_body() {
        dstring tmp = content;
        content = dstring{};
        return tmp;
    }

    std::pair<dstring, dstring> get_trailer() {
        auto trailer = trailers.front();
        trailers.pop();
        return trailer;
    };



private:
    bool ended{false};
    std::experimental::optional<error_callback_t> error_callback;
    std::experimental::optional<http_response> response_headers;
    dstring content;
    std::queue<std::pair<dstring, dstring>> trailers;
    std::function<void()> content_notification;

};

}

#endif //DOORMAT_RESPONSE_H

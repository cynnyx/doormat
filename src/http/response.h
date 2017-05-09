#ifndef DOORMAT_RESPONSE_H
#define DOORMAT_RESPONSE_H

#include <iostream>
#include <experimental/optional>
#include <functional>
#include <memory>
#include "../utils/dstring.h"
#include "http_response.h"

namespace http
{

class connection;

class response
{
    friend connection;
public:
    using error_callback_t = std::function<void()>;

    enum class state {
        pending,
        headers_received,
        body_received,
        trailer_received,
        ended
    };

    response(std::function<void()> content_notification);
    void headers(http_response &&res);
    void body(dstring &&d);
    void trailer(dstring &&k, dstring&& v);
    void end();

    void on_error(error_callback_t ecb);


    state get_state();
    http_response get_headers();
    dstring get_body();
    std::pair<dstring, dstring> get_trailer();
private:

    void error() {
        if(error_callback) (*error_callback)();
    }

    state current;
    bool ended{false};
    std::experimental::optional<error_callback_t> error_callback;
    std::experimental::optional<http_response> response_headers;
    dstring content;
    std::queue<std::pair<dstring, dstring>> trailers;
    std::function<void()> content_notification;

};

}

#endif //DOORMAT_RESPONSE_H

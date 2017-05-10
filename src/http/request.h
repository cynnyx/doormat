#ifndef DOORMAT_REQUEST_H
#define DOORMAT_REQUEST_H

#include <memory>
#include <functional>
#include <experimental/optional>
#include "http_request.h"

namespace http {
/** \brief the request class provides to the library user a mean through which it can subscribe to events related
 *  to the request.
 * */
class connection;
class request : public std::enable_shared_from_this<request> {
	friend class connection;
public:
    request(std::shared_ptr<connection>);
    /** Callback types. */
    using headers_callback_t = std::function<void(http::http_request &&)>;
    using body_callback_t = std::function<void(dstring&& d)>;
    using trailer_callback_t = std::function<void(dstring&&k, dstring&&v)>;
    using finished_callback_t = std::function<void()>;
    using error_callback_t = std::function<void()>;

    void on_headers(headers_callback_t);
    void on_body(body_callback_t);
    void on_error(error_callback_t);
    void on_trailer(trailer_callback_t);
    void on_finished(finished_callback_t);

private:

    /** External handlers allowing to trigger events to be communicated to the user */
    void headers(http::http_request &&);
    void body(dstring&& d);
    void error();
    void trailer(dstring&&, dstring&&);
    void finished();

    /* User registered events */
    std::experimental::optional<headers_callback_t> headers_callback;
    std::experimental::optional<body_callback_t> body_callback;
    std::experimental::optional<error_callback_t> error_callback;
    std::experimental::optional<trailer_callback_t> trailer_callback;
    std::experimental::optional<finished_callback_t> finished_callback;

    std::shared_ptr<connection> connection_keepalive;
};


}


#endif //DOORMAT_REQUEST_H

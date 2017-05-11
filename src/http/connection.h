#ifndef DOORMAT_CONNECTION_H
#define DOORMAT_CONNECTION_H

#include <functional>
#include <tuple>
#include <iostream>

#include "http_request.h"
#include "http_response.h"
#include "connection_error.h"

namespace http {

class request;
class response;

struct connection : std::enable_shared_from_this<connection> {
	using request_callback = std::function<void(connection&, std::shared_ptr<http::request>, std::shared_ptr<http::response>)>;
	using error_callback = std::function<void(const http::connection_error &)>;
    inline void on_request(request_callback rcb) { request_cb = std::move(rcb); }
	inline void on_error();
    virtual void set_persistent(bool persistent = true) = 0;
    virtual void close() = 0;
    virtual ~connection() = default;
    http::connection_error current{error_code::success};
protected:
	void request_received(std::shared_ptr<http::request>, std::shared_ptr<http::response>);
    inline void init(){ myself = this->shared_from_this(); }
    inline void deinit(){ myself = nullptr; }
private:
	request_callback request_cb;
    std::shared_ptr<connection> myself{nullptr};
};

}


#endif //DOORMAT_CONNECTION_H

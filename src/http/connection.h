#ifndef DOORMAT_CONNECTION_H
#define DOORMAT_CONNECTION_H

#include <functional>

#include "http_request.h"
#include "http_response.h"
namespace http {

class request;
class response;

struct connection : std::enable_shared_from_this<connection> {
	using request_callback = std::function<void(std::shared_ptr<http::request>, std::shared_ptr<http::response>)>;
	void on_request(request_callback rcb) { request_cb = std::move(rcb); }
	virtual void close() = 0;
	virtual ~connection() = default;
protected:
	void request_received(std::shared_ptr<http::request>, std::shared_ptr<http::response>);

	std::tuple<
	std::function<void()>,
	std::function<void(dstring&&)>,
	std::function<void(dstring&&, dstring&&)>,
	std::function<void()>
	> get_request_handlers();

private:
	request_callback request_cb;
};

}


#endif //DOORMAT_CONNECTION_H

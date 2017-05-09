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
	http_request *request_received();
	http_request *request_received(std::function<void(http_response&&)>, std::function<void(dstring&&)>, std::function<void(dstring&&, dstring&&)>, std::function<void()>);

	std::tuple<
	std::function<void()>,
	std::function<void(dstring&&)>,
	std::function<void(dstring&&, dstring&&)>,
	std::function<void()>
	> get_request_handlers();

	//TODO: make pure virtual as soon as we get to fix http2 implementation
	virtual void notify_response_headers(http_response &&res){}
	virtual void notify_response_body(dstring&& body){}
	virtual void notify_response_trailer(dstring&&k, dstring&&v){}
	virtual void notify_response_end(){}
private:


	/** Inner management of http events, so that we can send events to the request*/
	void notify_headers();
	void notify_body(dstring&& body);
	void notify_finished();
	void notify_trailers(dstring &&, dstring&&);
	void notify_error(bool global = false);


	bool poll_response(std::shared_ptr<response>);
	void notify_response();

	http_request current_request;
	std::queue<std::weak_ptr<http::request>> requests;
	std::queue<std::weak_ptr<http::response>> responses;
	request_callback request_cb;
};

}


#endif //DOORMAT_CONNECTION_H

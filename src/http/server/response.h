#ifndef DOORMAT_RESPONSE_H
#define DOORMAT_RESPONSE_H

#include <iostream>
#include <experimental/optional>
#include <functional>
#include <memory>
#include "../../utils/dstring.h"
#include "../http_response.h"
#include "../connection_error.h"

namespace server
{
template<typename handler_traits>
class handler_http1;
}

namespace http2 {
class stream;
}

namespace http
{
class response : public std::enable_shared_from_this<response>
{
	template<typename>
	friend class server::handler_http1;
	friend class http2::stream;
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
	response(std::function<void(http_response&&)>, std::function<void(dstring&&)>, std::function<void(dstring &&, dstring&&)>, std::function<void()>);

	void headers(http_response &&res);
	void body(dstring &&d);
	void trailer(dstring &&k, dstring&& v);
	void end();

	void on_error(error_callback_t ecb);


	state get_state() const noexcept;
	http_response get_preamble();
	dstring get_body();
	std::pair<dstring, dstring> get_trailer();

	~response() = default;
private:


    void error(http::connection_error err)
    {
        if(error_callback) (*error_callback)();
    }

	state current;
	bool ended{false};
	std::experimental::optional<error_callback_t> error_callback;
	std::experimental::optional<http_response> response_headers;
	dstring content;
	std::queue<std::pair<dstring, dstring>> trailers;
	std::function<void()> content_notification;


	std::function<void(http_response&&)> hcb;
	std::function<void(dstring&&)> bcb;
	std::function<void(dstring &&, dstring&&)> tcb;
	std::function<void()> ccb;

};

}

#endif //DOORMAT_RESPONSE_H

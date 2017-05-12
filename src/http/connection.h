#ifndef DOORMAT_CONNECTION_H
#define DOORMAT_CONNECTION_H

#include <functional>
#include <tuple>
#include <experimental/optional>
#include <chrono>

#include "http_request.h"
#include "http_response.h"
#include "connection_error.h"

namespace http {

class request;
class response;

struct connection : std::enable_shared_from_this<connection> {

	using request_callback = std::function<void(std::shared_ptr<connection>, std::shared_ptr<http::request>, std::shared_ptr<http::response>)>;
	using error_callback = std::function<void(std::shared_ptr<connection>, const http::connection_error &)>;
	using timeout_callback = std::function<void(std::shared_ptr<connection>)>;

	/** Register callbacks */
	void on_request(request_callback rcb);
	void on_error(error_callback);
	void on_timeout(std::chrono::milliseconds ms, timeout_callback);
	template<typename T>
	void on_timeout(T ms, timeout_callback tcb)
	{
		on_timeout(std::chrono::milliseconds{ms}, std::move(tcb));
	}

	/** Utilities provided to the user to manipulate the connection directly */
    virtual void set_persistent(bool persistent = true) = 0;
    virtual void close() = 0;

	virtual ~connection() = default;

protected:
	void request_received(std::shared_ptr<http::request>, std::shared_ptr<http::response>);
	void error(http::connection_error);

    inline void init(){ myself = this->shared_from_this(); }
    inline void deinit(){ myself = nullptr; }
	virtual void set_timeout(std::chrono::milliseconds) = 0;

private:
	http::connection_error current{error_code::success};
	request_callback request_cb;
	std::experimental::optional<timeout_callback> timeout_cb;
	std::experimental::optional<error_callback> error_cb;
    std::shared_ptr<connection> myself{nullptr};
};

}


#endif //DOORMAT_CONNECTION_H

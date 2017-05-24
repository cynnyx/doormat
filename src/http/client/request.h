#ifndef DOORMAT_CLIENT_REQUEST_H
#define DOORMAT_CLIENT_REQUEST_H

#include <experimental/optional>
#include <functional>
#include <memory>

#include "../http_request.h"
#include "../connection_error.h"

namespace server
{
template<typename handler_traits>
class handler_http1;

class connection;
}

namespace http2 {
class stream;
}

namespace http
{
class client_request : public std::enable_shared_from_this<client_request>
{
	template<typename>
	friend class server::handler_http1;
	friend class http2::stream;
public:
	using error_callback_t = std::function<void()>;
	using write_callback_t = std::function<void(std::shared_ptr<client_request>)>;
	using data_t = std::unique_ptr<const char[]>;

	enum class state {
		pending,
		headers_received,
		body_received,
		trailer_received,
		ended
	};

	client_request(std::function<void()> content_notification);
	client_request(std::function<void(http_request&&)>, std::function<void(data_t, size_t)>, std::function<void(std::string&&, std::string&&)>, std::function<void()>);

	void headers(http_request &&res);
	void body(data_t, size_t);
	void trailer(std::string&& k, std::string&& v);
	void end();

	void on_error(error_callback_t ecb);
	void on_write(write_callback_t wcb);

	http_request get_preamble();

private:
	state get_state() const noexcept;
	std::string get_body();
	std::pair<std::string, std::string> get_trailer();

	void error(http::connection_error err)
	{
		if(error_callback)
			error_callback();
	}

	void cleared()
	{
		if(write_callback)
			write_callback(this->shared_from_this());
	}

	state current;
	bool ended{false};

	error_callback_t error_callback;
	write_callback_t write_callback;

	std::experimental::optional<http_request> request_headers;
	std::string content;

	std::queue<std::pair<std::string, std::string>> trailers;
	std::function<void()> content_notification;

	std::function<void(http_request&&)> hcb;
	std::function<void(data_t, size_t)> bcb;
	std::function<void(std::string&&, std::string&&)> tcb;
	std::function<void()> ccb;

};

}

#endif // DOORMAT_CLIENT_REQUEST_H

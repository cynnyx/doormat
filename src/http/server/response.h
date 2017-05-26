#ifndef DOORMAT_RESPONSE_H
#define DOORMAT_RESPONSE_H

#include <iostream>
#include <experimental/optional>
#include <functional>
#include <memory>
#include <string>
#include <boost/asio/io_service.hpp>

#include "../http_response.h"
#include "../connection_error.h"

namespace server
{
template<typename handler_traits>
class handler_http1;
}

namespace http2 
{
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
	using write_callback_t = std::function<void(std::shared_ptr<response>)>;
	using data_t = std::unique_ptr<const char[]>;

	enum class state 
	{
		send_continue,
		pending,
		headers_received,
		body_received,
		trailer_received,
		ended
	};

	response(std::function<void()> content_notification, boost::asio::io_service&io);
	response(std::function<void(http_response&&)>, std::function<void(data_t, size_t)>, std::function<void(std::string&&, std::string&&)>, std::function<void()>, boost::asio::io_service&io);

	void headers(http_response &&res);
	void body(data_t d, size_t);
	void trailer(std::string&& k, std::string&& v);
	void end();
	void send_continue() { continue_required = true; notify_continue();  }
	void on_error(error_callback_t ecb);
	void on_write(write_callback_t wcb);

	state get_state() noexcept;
	http_response get_preamble();
	std::string get_body();
	std::pair<std::string, std::string> get_trailer();

private:
	std::shared_ptr<response> myself{nullptr};

    void error(http::connection_error err)
    {
	    ended = true;
	    if(error_callback)
		    io.post([self = this->shared_from_this()](){ self->error_callback();});
	    myself = nullptr;
    }

	void cleared()
	{
		ended = true;
		if(write_callback)
			io.post([self = this->shared_from_this()](){self->write_callback(self);});
		myself = nullptr;
	}

	state current;
	bool ended{false};
	bool continue_required{false};
	error_callback_t error_callback;
	write_callback_t write_callback;
	std::experimental::optional<http_response> response_headers;
	std::string content;
	std::queue<std::pair<std::string, std::string>> trailers;
	std::function<void()> content_notification;

	std::function<void(http_response&&)> hcb;
	std::function<void(data_t, size_t)> bcb;
	std::function<void(std::string&&, std::string&&)> tcb;
	std::function<void()> ccb;
	std::function<void()> notify_continue;


	boost::asio::io_service& io;
};

}

#endif //DOORMAT_RESPONSE_H

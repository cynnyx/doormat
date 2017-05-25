#ifndef DOORMAT_CLIENT_CONNECTION_H
#define DOORMAT_CLIENT_CONNECTION_H

#include <memory>
#include <functional>
#include <utility>

#include "../connection.h"
#include "request.h"

namespace http {

class client_response;

class client_connection : public connection
{
	using inverted_handlers_t = std::pair<std::shared_ptr<http::client_response>, std::shared_ptr<http::client_request>>;
public:
	using request_sent_callback = std::function<void(std::shared_ptr<client_connection>)>;
	using handlers_t = std::pair<std::shared_ptr<http::client_request>, std::shared_ptr<http::client_response>>;

	/** Register callbacks*/
	void on_request_sent(request_sent_callback rscb) { request_cb.emplace(std::move(rscb)); }
	handlers_t create_transaction()
	{
		auto p = get_user_handlers();
		return std::make_pair(std::move(p.second), std::move(p.first));
	}

protected:
	virtual inverted_handlers_t get_user_handlers() = 0;
	void cleared() override 
	{ 
		if(request_cb) 
			(*request_cb) ( std::static_pointer_cast<client_connection>(this->shared_from_this()) ); 
	}

private:
	std::experimental::optional<request_sent_callback> request_cb;
};

} // namespace http

#endif // DOORMAT_CLIENT_CONNECTION_H

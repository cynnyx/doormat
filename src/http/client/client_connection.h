#ifndef DOORMAT_CLIENT_CONNECTION_H
#define DOORMAT_CLIENT_CONNECTION_H

#include <memory>
#include <functional>

#include "../connection.h"

namespace http {

class client_response;

class client_connection : public connection
{
public:
	using response_callback = std::function<void(std::shared_ptr<client_connection>, std::shared_ptr<http::client_response>)>;

	/** Register callbacks */
	void on_response(response_callback rcb);

protected:
	void response_received(std::shared_ptr<http::client_response>);

private:
	response_callback response_cb;
};

} // namespace http

#endif // DOORMAT_CLIENT_CONNECTION_H

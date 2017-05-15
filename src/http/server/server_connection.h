#ifndef DOORMAT_SERVER_CONNECTION_H
#define DOORMAT_SERVER_CONNECTION_H

#include <memory>
#include <functional>

#include "../connection.h"

namespace http {

class request;
class response;

class server_connection : public connection
{
public:
	using request_callback = std::function<void(std::shared_ptr<server_connection>, std::shared_ptr<http::request>, std::shared_ptr<http::response>)>;

	/** Register callbacks */
	void on_request(request_callback rcb);

protected:
	void request_received(std::shared_ptr<http::request>, std::shared_ptr<http::response>);

private:
	request_callback request_cb;
};

} // namespace http

#endif // DOORMAT_SERVER_CONNECTION_H

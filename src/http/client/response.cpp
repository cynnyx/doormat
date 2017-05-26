#include "response.h"
#include "../client/client_connection.h"

namespace http {

/** Ctor */
client_response::client_response(std::shared_ptr<client_connection> conn, boost::asio::io_service &io) : connection_keepalive{std::move(conn)}, io{io}
{}

/** Events registration. */

void client_response::on_headers(headers_callback_t hcb)
{
	headers_callback = std::move(hcb);
}

void client_response::on_body(body_callback_t bcb)
{
	body_callback = std::move(bcb);
}

void client_response::on_trailer(trailer_callback_t tcb)
{
	trailer_callback = std::move(tcb);
}

void client_response::on_finished(finished_callback_t fcb)
{
	finished_callback = std::move(fcb);
}

void client_response::on_response_continue(continue_callback_t ccb)
{
	continue_callback = std::move(ccb);
}

void client_response::on_error(error_callback_t ecb)
{
	error_callback = std::move(ecb);
}

/** Events triggers */
void client_response::headers(http::http_response &&res)
{
	_preamble = std::move(res);
	if(headers_callback)
		io.post(std::bind(headers_callback, this->shared_from_this()));
}

void client_response::body(data_t d, size_t s)
{
	if(body_callback)
		io.post([self = this->shared_from_this(), _d = d.release(), s](){
			self->body_callback(self, data_t{_d}, s);
		});

}

void client_response::trailer(std::string&& k, std::string&& v)
{
	if(trailer_callback)
		io.post([self = this->shared_from_this(), k = std::move(k), v = std::move(v)](){
			self->trailer_callback(self, std::string(k), std::string(v));
		});
}

void client_response::finished()
{
	if(response_ended) return;
	if(finished_callback)
		io.post([self = this->shared_from_this()]()
		        {
			        self->finished_callback(self);
			        self->response_ended = true;
		        });

}

void client_response::response_continue()
{
	if(response_ended) return;
	if(continue_callback)
		io.post(std::bind(continue_callback, this->shared_from_this()));
}

void client_response::error(http::connection_error err)
{
	if(response_ended) return;
	conn_error = std::move(err);
	if(error_callback)
		io.post([self = this->shared_from_this()](){
			self->error_callback(self, self->conn_error);
			self->response_ended = true;
		});

}

std::shared_ptr<client_connection> client_response::get_connection()
{
	return connection_keepalive;
}

}

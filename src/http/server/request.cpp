#include "request.h"
#include "../server/server_connection.h"

namespace http {

/** Ctor */
request::request(std::shared_ptr<server_connection> conn, boost::asio::io_service &io) : connection_keepalive{std::move(conn)}, io{io}
{}

/** Events registration. */

void request::on_headers(headers_callback_t hcb)
{
	headers_callback = std::move(hcb);
}

void request::on_body(body_callback_t bcb)
{
	body_callback = std::move(bcb);
}

void request::on_trailer(trailer_callback_t tcb)
{
	trailer_callback = std::move(tcb);
}

void request::on_finished(finished_callback_t fcb)
{
	finished_callback = std::move(fcb);
}

void request::on_error(error_callback_t ecb)
{
	error_callback = std::move(ecb);
}

/** Events triggers */
void request::headers(http::http_request &&req)
{
	_preamble = std::move(req);
	if(headers_callback)
		io.post([self = this->shared_from_this()](){
			self->headers_callback(self);
		});

}

void request::body(data_t d, size_t s)
{
	if(body_callback) //the lambda must be copy-able. Hence we use this cheap trick of releasing the ownership of the unique ptr, but just for a while.
		io.post([self = this->shared_from_this(), _d = d.release(), s = std::move(s)]()
				{
					self->body_callback(self, data_t{_d}, s);
				});
}

void request::trailer(std::string&& k, std::string&& v)
{
	if(trailer_callback)
		io.post([self = this->shared_from_this(), k = std::move(k), v = std::move(v)](){
			self->trailer_callback(self, std::string(k), std::string(v));
		});
}

void request::finished()
{
	if(request_terminated) return;
	if(finished_callback)
		io.post([self = this->shared_from_this()](){
			self->finished_callback(self);
			self->request_terminated = true;
		});

}

void request::error(http::connection_error err)
{
	if(request_terminated) return;
	conn_error = std::move(err);
	if(error_callback)
		io.post([self = this->shared_from_this()](){
			self->error_callback(self, self->conn_error);
			self->request_terminated = true;
		});
}

std::shared_ptr<server_connection> request::get_connection()
{
	return connection_keepalive;
}

}

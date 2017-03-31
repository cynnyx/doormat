#ifndef CLIENT_WRAPPER_H
#define CLIENT_WRAPPER_H

#include "../network/receiver.h"
#include "../network/connector.h"
#include "../chain_of_responsibility/node_interface.h"

namespace client
{
	
class client_wrapper : public node_interface
{
	class cw_receiver: public network::receiver
	{
		client_wrapper& cw;
		bool dead{false};
	public:
		cw_receiver( client_wrapper& self ): cw(self){}
		void on_header(http::http_response&& resp) noexcept override;
		void on_body(dstring&& ) noexcept override;
		void on_trailer(dstring&& , dstring && ) noexcept override;
		void on_error(int error) noexcept override;
		void on_eom() noexcept override;
		void stop() noexcept override;
	};
	
	std::shared_ptr<network::receiver> recv;
	std::shared_ptr<network::connector> connection;
public:
	using node_interface::node_interface;
	
	// This is the request to make - not the one received!
	void on_request_preamble(http::http_request&& message);
	void on_request_body(dstring&& chunk);
	void on_request_trailer(dstring&& k, dstring&& v);
	void on_request_canceled(const errors::error_code &err);
	void on_request_finished();
	~client_wrapper();
};

}
#endif // CLIENT_WRAPPER_H

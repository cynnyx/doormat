#ifndef DOOR_MAT_CLIENT_WRAPPER_H
#define DOOR_MAT_CLIENT_WRAPPER_H

#include "../chain_of_responsibility/node_interface.h"
#include "../errors/error_codes.h"
#include "../log/access_record.h"
#include "../network/cloudia_pool.h"
#include "../http_client.h"
#include "../network/communicator/dns_communicator_factory.h"

#include <chrono>

namespace http
{
class client_request;
}

namespace nodes
{

class client_wrapper : public node_interface
{
	using node_interface::node_interface;

	std::chrono::time_point<std::chrono::high_resolution_clock> start;
public:
	client_wrapper(req_preamble_fn request_preamble,
		req_chunk_fn request_body,
		req_trailer_fn request_trailer,
		req_canceled_fn request_canceled,
		req_finished_fn request_finished,
		header_callback hcb,
		body_callback body,
		trailer_callback trailer,
		end_of_message_callback end_of_message,
		error_callback ecb,
		response_continue_callback rccb,
		logging::access_recorder *aclogger = nullptr);

	/** events from the chain to which the client wrapper responds. */
	void on_request_preamble(http::http_request&& message);
	void on_request_body(dstring&& chunk);
	void on_request_trailer(dstring&& k, dstring&& v);
	void on_request_canceled(const errors::error_code &err);
	void on_request_finished();

	~client_wrapper() override;

private:
	void perform_request(http::http_request&& preamble);
	/**
	 * @brief termination_handler checks whether a termination condition (an error or an EOM)
	 * has been triggered and handles the communication of the relative event back to the chain.
	 *
	 * As it can possibly call the on_error() or on_end_of_message() member functions,
	 * it may destroy the whole chain and the current instance of client_wrapper,
	 * so be carefull.
	 *
	 * @return true if at least a termination condition holds, false otherwise.
	 */
	void termination_handler();

	void stop();

	client::http_client<network::dns_connector_factory> client;
	bool stopping{false};
	bool finished_request{false};
	bool finished_response{false};
	bool managing_continue{false}; //current state; used to manage HTTP 1.1 / 100 status codes.
	bool canceled{false};

	errors::error_code errcode;
	uint8_t connection_attempts{0};

	//used in order to manage connect.
	std::shared_ptr<http::client_connection> connection;
	std::shared_ptr<http::client_request> local_request;
	dstring tmp_body;

	uint8_t waiting_count{0};
	std::unique_ptr<network::socket_factory<boost::asio::ip::tcp::socket>> factory;
};



} // namespace nodes


#endif //DOOR_MAT_CLIENT_WRAPPER_H

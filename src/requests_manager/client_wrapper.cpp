#include "client_wrapper.h"
#include "../io_service_pool.h"
#include "../service_locator/service_locator.h"
#include "../board/abstract_destination_provider.h"
#include "../configuration/configuration_wrapper.h"
#include "../stats/stats_manager.h"
#include "../utils/log_wrapper.h"
#include "../constants.h"
#include "../network/magnet.h"
#include "../http/http_commons.h"
#include <limits>

namespace nodes
{

client_wrapper::client_wrapper (
		req_preamble_fn request_preamble,
		req_chunk_fn request_body,
		req_trailer_fn request_trailer,
		req_canceled_fn request_canceled,
		req_finished_fn request_finished,
		header_callback hcb,
		body_callback bcb,
		trailer_callback tcb,
		end_of_message_callback eomcb,
		error_callback ecb,
		response_continue_callback rccb,
		logging::access_recorder *aclogger)
	: node_interface(std::move(request_preamble), std::move(request_body), std::move(request_trailer),
					 std::move(request_canceled), std::move(request_finished),
					 std::move(hcb), std::move(bcb), std::move(tcb), std::move(eomcb),
					 std::move(ecb), std::move(rccb), aclogger)
	, codec{}
	, received_parsed_message{}
{
	LOGTRACE("client_wrapper ",this," constructor");
/** Response callbacks: used to return control by the decoder.  **/

	auto codec_scb = [this](http::http_structured_data** data)
	{
		if(managing_continue)  //reset to initial state.
		{
			managing_continue = false;
			received_parsed_message = http::http_response{};
		}
		LOGTRACE("client_wrapper ",this," codec start cb triggered");
		*data = &received_parsed_message;
	};

	auto codec_hcb = [this]()
	{
		assert(!finished_response);
		LOGTRACE("client_wrapper ",this," codec header cb triggered");
		if (received_parsed_message.status_code() == 100)
		{
			managing_continue = true;
			//continue management;
			return;
		}
		received_parsed_message.set_destination_header(addr);
		on_header(std::move(received_parsed_message));
	};

	auto codec_bcb = [this](dstring&& d)
	{
		assert(!finished_response);
		LOGTRACE("client_wrapper ",this," codec body cb triggered");
		if(!managing_continue) on_body(std::move(d));
	};

	auto codec_tcb = [this](dstring&& k,dstring&& v)
	{
		assert(!finished_response);
		LOGTRACE("client_wrapper ",this," codec trailer cb triggered");
		if(!managing_continue) on_trailer(std::move(k),std::move(v));
	};

	auto codec_ccb = [this]()
	{
		assert(!finished_response);
		LOGTRACE("client_wrapper ",this," codec completion cb triggered");
		if(managing_continue)
		{
			return on_response_continue();
		}
		finished_response = true;
		stop(); //we already received our response; hence we can stop the client wrapper.
	};

	auto codec_fcb = [this](int err, bool& fatal)
	{
		LOGTRACE("client_wrapper ",this," codec error cb triggered");
		switch(err)
		{
			case HPE_UNEXPECTED_CONTENT_LENGTH:
				fatal = false;
				codec.ingnore_content_len();
				break;
			default:
				fatal = true;
		}
		errcode = INTERNAL_ERROR_LONG(errors::http_error_code::internal_server_error);
		stop();
	};

	codec.register_callback(std::move(codec_scb), std::move(codec_hcb), std::move(codec_bcb), std::move(codec_tcb),
		std::move(codec_ccb), std::move(codec_fcb));
}

client_wrapper::~client_wrapper()
{
	assert(waiting_count == 0);
	LOGTRACE("client_wrapper ",this," destructor");
}

routing::abstract_destination_provider::address client_wrapper::get_custom_address(const http::http_request& _preamble)
{
	routing::abstract_destination_provider::address _addr;
	_addr.ipv6(std::string(_preamble.header( http::hf_cyn_dest )));
	if (_addr.ipv6().to_string().empty()) return {}; //default, invalid address.
	dstring head = _preamble.header( http::hf_cyn_dest_port );
	uint16_t port_number;

	if ( !head.to_integer(port_number) ) return {};
	_addr.port(static_cast<uint16_t>( port_number ));
	return _addr;
}


void client_wrapper::connect()
{
	if( (custom_addr && connection_attempts != 0) ||
		(++connection_attempts > service::locator::configuration().get_max_connection_attempts()))
	{
		LOGDEBUG("client wrapper", this, " failed to contact 3 different destinations in a row; returning error 500");
		errcode = INTERNAL_ERROR_LONG(errors::http_error_code::service_unavailable);
		//on_error(errcode);
		stop();
		termination_handler();
		return;
	}
	++waiting_count;
	LOGTRACE("client wrapper ", this, " asking for a socket");
	if(custom_addr)
	{
		service::locator::socket_pool().get_socket(addr, [this](std::unique_ptr<boost::asio::ip::tcp::socket> socket)
		{
			--waiting_count;
			if(!socket) return connect();
			LOGTRACE("client wrapper ", this, " successfully retrieved a socket.");
			on_connect(std::move(socket));
		});
		return;
	}
	service::locator::socket_pool().get_socket(local_request, [this](std::unique_ptr<boost::asio::ip::tcp::socket> socket)
	{
		--waiting_count;
		if(!socket)
		{
			LOGDEBUG("failed to get socket at attempt ", connection_attempts);
			return connect();
		}
		LOGTRACE("client wrapper ", this, " successfully retrieved a socket.");
		on_connect(std::move(socket));
	});
	return;
}

void client_wrapper::on_request_preamble(http::http_request&& preamble)
{
	LOGTRACE("client_wrapper ",this," on_request_preamble");
	custom_addr = preamble.has(http::hf_cyn_dest);
	if(custom_addr)
	{
		addr = get_custom_address(preamble);
	}
	start = std::chrono::high_resolution_clock::now();
	local_request = std::move(preamble);
	connect();
	write_proxy.enqueue_for_write(codec.encode_header(local_request));
}


void client_wrapper::on_connect(std::unique_ptr<boost::asio::ip::tcp::socket> socket)
{
	assert(socket);
	//create a new communicator and give it to the write_proxy.
	++waiting_count;
	auto communicator = new network::communicator(std::move(socket), [this](const char *data, size_t size)
	{
		if(!codec.decode(data, size))
		{
			errcode = INTERNAL_ERROR_LONG( errors::http_error_code::internal_server_error );
			stop();
		}
	},[this](errors::error_code errc)
	{
		--waiting_count;
		if(errc.code() > 1)
			errcode = errc; //todo: this is a porchetta.
		stop();
		termination_handler();
	});
	write_proxy.set_communicator(communicator);
	return;
}

void client_wrapper::on_request_body(dstring&& chunk)
{
	LOGTRACE("client_wrapper ",this," on_request_body");
	if(!errcode)
	{
		write_proxy.enqueue_for_write(codec.encode_body(chunk));
	}
}

void client_wrapper::on_request_trailer(dstring&& k, dstring&& v)
{
	if(!errcode)
	{
		write_proxy.enqueue_for_write(codec.encode_trailer(k,v));
	}
}

void client_wrapper::on_request_finished()
{
	LOGTRACE("client_wrapper ",this," on_request_finished");
	finished_request = true;
	if(!errcode)
	{
		write_proxy.enqueue_for_write(codec.encode_eom());
	}
	//in some cases, response could have arrived before the end of the request; in this case, we shall react accordingly.
	if(finished_response)
		termination_handler();
}

void client_wrapper::termination_handler()
{
	if( !stopping ) return;

	if(waiting_count > 0)
		return LOGTRACE("client_wrapper ",this," waiting for ", waiting_count, " lambdas");

	auto end = std::chrono::high_resolution_clock::now();
	auto latency = end - start;

	uint64_t nanoseconds = std::chrono::nanoseconds(latency).count();
	service::locator::stats_manager().enqueue( stats::stat_type::board_response_latency, 
		static_cast<double>( nanoseconds ) );

	if(finished_response && finished_request) //notice that with the new modification finished_request check is probably not needed anymore; however, it is safer and costs nothing.
	{
		LOGTRACE("client_wrapper ",this," End of message latency: ", static_cast<double>( nanoseconds ) );
		return on_end_of_message();
	}

	if(errcode && (finished_request || canceled))
	{
		LOGTRACE("client_wrapper ",this," Error:", errcode);
		return on_error(errcode);
	}
}

void client_wrapper::on_request_canceled(const errors::error_code &ec)
{
	LOGDEBUG("client_wrapper ", this, " operations canceled from upstream with error ", ec);
	errcode = ec;
	canceled = true;
	stop();
}


void client_wrapper::stop()
{
	LOGTRACE("client_wrapper ",this," stop!");
	if(!stopping)
	{
		write_proxy.shutdown_communicator();
		LOGDEBUG("client_wrapper ",this," stopping");
		boost::system::error_code ec;
		stopping = true;
	}
}

} // namespace nodes

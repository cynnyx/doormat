#include "client_wrapper.h"
#include "../io_service_pool.h"
#include "../service_locator/service_locator.h"
#include "../configuration/configuration_wrapper.h"
#include "../stats/stats_manager.h"
#include "../utils/log_wrapper.h"
#include "../constants.h"
#include "../http/http_commons.h"
#include "../network/communicator/communicator_factory.h"
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
		//received_parsed_message.set_destination_header(addr);
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

void client_wrapper::on_request_preamble(http::http_request&& preamble)
{
	LOGTRACE("client_wrapper ",this," on_request_preamble");
	start = std::chrono::high_resolution_clock::now();
	local_request = std::move(preamble);
    ++waiting_count;
    //retrieve a connector to talk with remote destination.
	service::locator::communicator_factory().get_connector(local_request, [this](std::shared_ptr<network::communicator_interface> ci){
        --waiting_count;
        LOGTRACE("client_wrapper ", this, " received communicator");
        ci->set_callbacks([this](const char *data, size_t size)
                          {
                              if(!codec.decode(data, size))
                              {
                                  errcode = INTERNAL_ERROR_LONG( errors::http_error_code::internal_server_error );
                                  stop();
                              }
                          },[this](errors::error_code errc)
                          {
                              if(errc.code() > 1)
                                  errcode = errc; //todo: this is a porchetta.
                              stop();
                              termination_handler();
                          });

        write_proxy.set_communicator(ci);
	}, [this](int e){
        LOGTRACE("client_wrapper ", this, " could not retrieve a communicator for the specified endpoint");
        --waiting_count;
        errcode = INTERNAL_ERROR_LONG(errors::http_error_code::internal_server_error);
        canceled = true;
        stop();
        termination_handler();
	});
	write_proxy.enqueue_for_write(codec.encode_header(local_request));
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
		return;
	}
	//in some cases, response could have arrived before the end of the request; in this case, we shall react accordingly.
	if(finished_response)
		termination_handler();
}

void client_wrapper::termination_handler()
{
	if( !stopping ) return;

	if(waiting_count > 0)
		return LOGTRACE("client_wrapper ",this," waiting for ", (int) waiting_count, " lambdas");

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
        std::cout << "sending back an error!" << std::endl;
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
		stopping = true;
	}
}

} // namespace nodes

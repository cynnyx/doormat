#include "../http/client/client_connection.h"
#include "../http/client/request.h"
#include "../http/client/response.h"
#include "client_wrapper.h"
#include "../io_service_pool.h"
#include "../service_locator/service_locator.h"
#include "../configuration/configuration_wrapper.h"
#include "../stats/stats_manager.h"
#include "../utils/log_wrapper.h"
#include "../constants.h"
#include "../http/http_commons.h"
#include "../network/communicator/communicator_factory.h"
#include "../connector.h"
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
	, client{service::locator::service_pool().get_thread_io_service(),
			 std::chrono::milliseconds{service::locator::configuration().get_board_timeout()}}
{
	LOGTRACE("client_wrapper ",this," constructor");
}

client_wrapper::~client_wrapper()
{
	assert(waiting_count == 0);
	LOGTRACE("client_wrapper ",this," destructor");
	stop();
}

void client_wrapper::perform_request(http::http_request&& preamble)
{
	auto p = connection->create_transaction();
	this->local_request = std::move(p.first);
	this->local_request->headers(std::move(preamble));
	// TODO: again... copying
	auto ptr = std::make_unique<char[]>(tmp_body.size());
	std::copy(tmp_body.cbegin(), tmp_body.cend(), ptr.get());
	this->local_request->body(std::move(ptr), tmp_body.size());
	auto& res = p.second;
	res->on_headers([this](auto res)
	{
		assert(!this->finished_response);
		LOGTRACE("client_wrapper ",this," response headers cb triggered");
		this->managing_continue = res->preamble().status_code() == 100 ? true : false;
		this->on_header(std::move(res->preamble()));
	});
	res->on_body([this](auto res, auto data, auto length)
	{
		assert(!finished_response);
		LOGTRACE("client_wrapper ",this," response body cb triggered");
		if(!managing_continue)
			this->on_body(std::move(data), length);
	});
	res->on_trailer([this](auto res, auto k, auto v)
	{
		assert(!finished_response);
		LOGTRACE("client_wrapper ",this," response trailer cb triggered");
		if(!managing_continue)
			this->on_trailer(dstring{k.data(), k.size()}, dstring{v.data(), v.size()});
	});
	res->on_finished([this](auto res)
	{
		assert(!finished_response);
		LOGTRACE("client_wrapper ",this," response completion cb triggered");
		if(managing_continue)
		{
			return this->on_response_continue();
		}
		finished_response = true;
		this->on_end_of_message();
		this->stop(); //we already received our response; hence we can stop the client wrapper.
	});
	res->on_error([this](auto res, auto&& error)
	{
		LOGTRACE("client_wrapper ",this," response error cb triggered: ", error.message());
		errcode = INTERNAL_ERROR_LONG(errors::http_error_code::internal_server_error);
		this->stop();
	});
}

void client_wrapper::on_request_preamble(http::http_request&& preamble)
{
	LOGTRACE("client_wrapper ",this," on_request_preamble");
	start = std::chrono::high_resolution_clock::now();
//	++waiting_count;
	//retrieve a connector to talk with remote destination.
	if(!preamble.hasParameter("hostname"))
		return on_error(INTERNAL_ERROR_LONG(errors::http_error_code::unprocessable_entity));

	auto address = preamble.getParameter("hostname");
	auto port = preamble.hasParameter("port") && !preamble.getParameter("port").empty() ?
				std::stoi(preamble.getParameter("port")) : 0;
	auto tls = preamble.ssl();
	auto proto_v = preamble.protocol_version();

	if(connection)
		return perform_request(std::move(preamble));

	client.connect([this, preamble=std::move(preamble)](auto connection_ptr) mutable
	{
		connection = std::move(connection_ptr);
		this->perform_request(std::move(preamble));
	}, [this](auto&& error)
	{
		LOGTRACE("client_wrapper ",this," connect error cb triggered");
		errcode = INTERNAL_ERROR_LONG(errors::http_error_code::internal_server_error);
		this->stop();
	}, proto_v, address, static_cast<uint16_t>(port), tls);
}

void client_wrapper::on_request_body(data_t data, size_t len)
{
	LOGTRACE("client_wrapper ",this," on_request_body");
	if(!errcode)
	{
		if(local_request)
		{
			local_request->body(std::move(data), len);
		}
		else
			tmp_body.append(data.get(), len);
	}
}

void client_wrapper::on_request_trailer(std::string&& k, std::string&& v)
{
	if(!errcode)
	{
		local_request->trailer(std::move(k), std::move(v));
	}
}

void client_wrapper::on_request_finished()
{
	LOGTRACE("client_wrapper ",this," on_request_finished");
	finished_request = true;
	if(!errcode)
	{
		if(local_request)
			local_request->end();
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
		if(connection)
			connection->close();
		LOGDEBUG("client_wrapper ",this," stopping");
		stopping = true;
	}
}

} // namespace nodes

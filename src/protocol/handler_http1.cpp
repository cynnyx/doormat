#include "handler_http1.h"
#include "../http/http_commons.h"
#include "../connector.h"
#include "../utils/log_wrapper.h"
#include "../log/inspector_serializer.h"
#include "../service_locator/service_locator.h"

#include <typeinfo>

namespace server
{

handler_http1::handler_http1(http::proto_version version)
	: version{version}
{
	LOGINFO("HTTP1 selected");
}

bool handler_http1::some_message_started( http::proto_version& proto ) const noexcept
{
	return std::any_of(th.begin(), th.end(), [](const transaction_handler& t){ return t.message_started; } );
}

bool handler_http1::start() noexcept
{
	auto scb = [this](http::http_structured_data** data)
	{
		th.emplace_back(std::move(handler_interface::make_chain()), this, connector()->is_ssl() );
		*data = &(th.back().get_data());
		(*data)->origin( find_origin() );
	};

	auto hcb = [this]()
	{
		auto&& current_transaction = th.back();
		auto&& data = current_transaction.get_data();
		persistent_connection = current_transaction.persistent = data.keepalive();
		version = (version == http::proto_version::UNSET) ? data.protocol_version() : version;
		current_transaction.on_request_preamble(std::move(data));
	};

	auto bcb = [this](dstring&& b)
	{
		auto&& current_transaction = th.back();
		// pass the request preamble down the chain
// 		current_transaction.cor->on_request_body(std::move(b));
		current_transaction.on_request_body(std::move(b));
	};

	auto tcb = [this](dstring&& k, dstring&& v)
	{
		auto&& current_transaction = th.back();
		// pass the request preamble down the chain
		current_transaction.on_request_trailer( std::move(k), std::move(v) );
	};

	auto ccb = [this]()
	{
		auto&& current_transaction = th.back();
		// the request is complete
		current_transaction.on_request_finished();
	};

	auto fcb = [this](int error,bool&)
	{
		LOGTRACE("Codec failure handling");
		/*http::proto_version pv = version;
		if ( ! error_code_distruction && some_message_started( pv ) )
		{
			switch ( error )
			{
				case HPE_CB_url:
					errors::error_factory_async::error_response( ne,
						static_cast<uint16_t>( errors::http_error_code::uri_too_long ), pv);
					break;

				case HPE_HEADER_OVERFLOW:
				case HPE_INVALID_EOF_STATE:
				case HPE_INVALID_VERSION:
				case HPE_INVALID_STATUS:
				case HPE_INVALID_METHOD:
				case HPE_INVALID_URL:
				case HPE_INVALID_HOST:
				case HPE_INVALID_PORT:
				case HPE_INVALID_PATH:
				case HPE_INVALID_QUERY_STRING:
				case HPE_INVALID_FRAGMENT:
				case HPE_LF_EXPECTED:
				case HPE_INVALID_HEADER_TOKEN:
				case HPE_INVALID_CONTENT_LENGTH:
				case HPE_UNEXPECTED_CONTENT_LENGTH:
				case HPE_INVALID_CHUNK_SIZE:
				case HPE_INVALID_CONSTANT:
				case HPE_INVALID_INTERNAL_STATE:
					errors::error_factory_async::error_response( ne,
						static_cast<uint16_t>( errors::http_error_code::bad_request ), pv);
					break;
				case HPE_STRICT:
				case HPE_PAUSED:
				case HPE_CLOSED_CONNECTION:
				default:
					break;
			}
		}*/
		//TODO: manage HTTP1 error.
	};

	decoder.register_callback(std::move(scb), std::move(hcb), std::move(bcb), std::move(tcb), std::move(ccb), std::move(fcb));
	return true;
}

bool handler_http1::should_stop() const noexcept
{
	auto finished = true;
	for ( auto &open_transaction: th )
	{
		if(!open_transaction.message_ended || open_transaction.has_encoded_data())
		{
			finished = false;
			break;
		}
	}
	return error_happened || (finished && !persistent_connection);
}

bool handler_http1::on_read(const char* data, size_t len)
{
	LOGTRACE(this, " Received chunk of size:", len);
	auto rv = decoder.decode(data, len);
	connector()->_rb.consume(data + len );
	return rv;
}

bool handler_http1::on_write(dstring& data)
{
	if(connector())
	{
		if(!th.empty() && th.front().has_encoded_data())
			data = std::move( th.front().get_encoded_data() );
		else if(!th.empty() && th.front().disposable())
			th.pop_front();
		return true;
	}
	return false;
}

void handler_http1::transaction_handler::on_header(http::http_response&& preamble)
{
	access.response( preamble );
	LOGTRACE(this," transaction_handler::on_header");
	message_started = true;
	preamble.keepalive(persistent);
	encoded_data.append(encoder.encode_header(preamble));
	enclosing->notify_write();
}

void handler_http1::transaction_handler::on_body(dstring&& chunk)
{
	LOGTRACE(this," transaction_handler::on_body");
	assert(chunk.size());
	if ( service::locator::inspector_log().active() ) access.append_response_body( chunk );
	access.add_request_size(chunk.size());
	encoded_data.append(encoder.encode_body(chunk));
	enclosing->notify_write();
}

void handler_http1::transaction_handler::on_trailer(dstring&& k, dstring&& v)
{
	LOGTRACE(this," transaction_handler::on_trailer");
	assert( k.size() && v.size() );
	if ( service::locator::inspector_log().active() ) access.append_response_trailer(k, v);
	encoded_data.append(encoder.encode_trailer(k,v));
	enclosing->notify_write();
}

void handler_http1::transaction_handler::on_eom()
{
	LOGTRACE(this," transaction_handler::on_eom");
	access.set_request_end();
	encoded_data.append(encoder.encode_eom());
	enclosing->notify_write();
	message_ended = true;
	enclosing->on_eom();
}

void handler_http1::on_eom()
{
	LOGTRACE(this, " on_eom");
	if(!connector())
	{
		th.remove_if( [](transaction_handler &t){ return t.message_ended; });
		if(th.empty())
			delete this;
	}
}

void handler_http1::on_error(const int&)
{
	LOGTRACE(this, " on_error");
	error_happened = true;
	if(!connector())
	{
		th.remove_if([](transaction_handler &t){ return t.message_ended; });
		if(th.empty()) delete this;
	}
}

void handler_http1::transaction_handler::on_request_preamble(http::http_request&& message)
{
	access.request(message);
	if ( enclosing && enclosing->th.size() > 1 ) access.set_pipe( true );
	LOGTRACE(this," on_request_preamble");
	cor->on_request_preamble(std::move(message));
}

void handler_http1::transaction_handler::on_response_continue()
{
	LOGTRACE(this," on_response_continue");
	access.continued();
	continue_response.status(100);  //continue code
	continue_response.protocol(http::proto_version::HTTP11);
	continue_response.keepalive(persistent);
	encoded_data.append(encoder.encode_header(continue_response));
	encoded_data.append(encoder.encode_eom());
	enclosing->notify_write();
}

void handler_http1::transaction_handler::on_error(const int &ec)
{
	LOGTRACE(this," :on_error");
	access.error( ec );
	access.commit();
	message_ended = true;
	encoded_data = {};
	enclosing->on_error(ec);
}

void handler_http1::transaction_handler::on_request_canceled(const errors::error_code &ec)
{
	LOGTRACE(this," transaction_handler::on_request_canceled");
	access.cancel();
	cor->on_request_canceled(ec);
}

void handler_http1::do_write()
{
	if(connector())
		connector()->do_write();
	else
		LOGERROR("handler_factory ", this, " connector already destroyed");
}

void handler_http1::on_connector_nulled()
{
	error_code_distruction = INTERNAL_ERROR_LONG(408);
	if(handler_http1::should_stop() || th.empty())
		delete this;
	else
		for (auto &t : th) t.on_request_canceled(error_code_distruction);
}

} //namespace

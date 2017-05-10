#include "handler_http1.h"
#include "../http/http_commons.h"
#include "../connector.h"
#include "../utils/log_wrapper.h"
#include "../log/inspector_serializer.h"
#include "../service_locator/service_locator.h"
#include "../endpoints/chain_factory.h"
#include "../http/request.h"
#include "../http/response.h"

#include <typeinfo>

namespace server
{

handler_http1::handler_http1(http::proto_version version)
	: version{version}
{
	LOGINFO("HTTP1 selected");
}

bool handler_http1::start() noexcept
{
    auto scb = [this](http::http_structured_data** data)
	{

		auto req = std::make_shared<http::request>(this->shared_from_this());
		auto res = std::make_shared<http::response>([self = this->shared_from_this(), this](){
			notify_response();
		});
        *data = &current_request;
		request_received(req, res);
		requests.emplace(std::move(req));
		responses.push(std::move(res));
	};

	auto hcb = [this]()
	{
		if(requests.empty()) return;
		if(auto s = requests.back().lock())
		{
			s->headers(std::move(current_request));
			current_request = http::http_request{};
			//todo: connection error
		}
	};

	auto bcb = [this](dstring&& b)
	{
		if(requests.empty()) return;
		if(auto s = requests.back().lock())
		{
			s->body(std::move(b));
			//todo: connection error
		}
	};

	auto tcb = [this](dstring&& k, dstring&& v)
	{
		if(requests.empty()) return;
		if(auto s = requests.back().lock()) {
			s->trailer(std::move(k), std::move(v));
		}
	};

	auto ccb = [this]()
	{
		if(requests.empty()) return;
		if(auto s = requests.back().lock()) {
			s->finished();
		}
	};

	auto fcb = [this](int error,bool&)
	{
		//failure always gets called after start...
		LOGTRACE("Codec failure handling");
		if(auto s = requests.back().lock())
		{
			s->error();
		}
		if(auto s = responses.back().lock()) {
			s->error();
		}
		requests.pop();
		responses.pop();
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
	auto finished = responses.empty();
	return error_happened || (finished && !persistent);
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
		if(!serialization.empty())
		{
			data = std::move(serialization);
			return true;
		}
	}
	return false;
}


void handler_http1::on_error(const int&)
{
	LOGTRACE(this, " on_error");
	error_happened = true;
	//propagate error.
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
	//should notify everybody in the connection of the error!

}


bool handler_http1::poll_response(std::shared_ptr<http::response> res) {
    auto state = res->get_state();
    while(state != http::response::state::pending) {
        switch(state) {
            case http::response::state::headers_received:
                notify_response_headers(res->get_headers()); break;
            case http::response::state::body_received:
                notify_response_body(res->get_body()); break;
            case http::response::state::trailer_received:
            {
                auto trailer = res->get_trailer();
                notify_response_trailer(std::move(trailer.first), std::move(trailer.second));
                break;
            }
            case http::response::state::ended:
                notify_response_end();
                return true;
            default: assert(0);
        }
        state = res->get_state();
    }
    return false;
}

//http1 only; move it down in the hierarchy.
void handler_http1::notify_response()
{
    while(!responses.empty() && !requests.empty())
    {
        //replace all this with a polling mechanism!
        auto &c = responses.front();
        if(auto s = c.lock()) {
            if(poll_response(s))
            {
                //we pop them both.
                responses.pop();
                requests.pop();
            }
            else break;
        }
    }
}


void handler_http1::notify_response_headers(http::http_response&& res)
{
    serialization.append(encoder.encode_header(res));
    do_write();
}

void handler_http1::notify_response_body(dstring&& b)
{
    serialization.append(encoder.encode_body(b));
    do_write();
}

void handler_http1::notify_response_trailer(dstring&&k, dstring&&v)
{
    serialization.append(encoder.encode_trailer(k, v));
    do_write();
}

void handler_http1::notify_response_end()
{
    serialization.append(encoder.encode_eom());
    do_write();
}


} //namespace

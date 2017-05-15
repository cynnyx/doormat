#pragma once

#include <memory>
#include "../http/http_codec.h"
#include "../http/http_structured_data.h"
#include "../log/access_record.h"
#include "../utils/log_wrapper.h"
#include "../http/server/request.h"
#include "../http/server/response.h"
#include "../connector.h"

// TODO: should not be in _server_ namespace
namespace server
{

template<typename handler_traits>
class handler_http1 final: public http_handler, public http::connection
{

public:
	using request_t = typename handler_traits::request_t;
	using response_t = typename handler_traits::response_t;
	using incoming_t = typename handler_traits::incoming_t;


	std::shared_ptr<handler_http1> get_shared()
	{
		return std::static_pointer_cast<handler_http1>(this->shared_from_this());
	}

	void trigger_timeout_event() override {
		timeout();
	};

	handler_http1(http::proto_version version)
		: version{version}
	{
		LOGINFO("HTTP1 selected");
	}

	bool start() noexcept override
	{
		init();
		auto scb = [this](http::http_structured_data** data)
		{

			auto req = std::make_shared<request_t>(this->shared_from_this());
			req->init();
			auto res = std::make_shared<response_t>([this, self = this->shared_from_this()](){
				notify_response();
			});
			*data = &current_request;
			requests.push_back(req);
			responses.push_back(res);

			request_received(std::move(req), std::move(res));
		};

		auto hcb = [this]()
		{
			if(requests.empty()) return;
			if(auto s = requests.back().lock())
			{
				io_service().post(
							[s, current_request = std::move(current_request)]() mutable {s->headers(std::move(current_request));}
				);
				current_request = {};
			}
		};

		auto bcb = [this](dstring&& b)
		{
			if(requests.empty()) return;
			if(auto s = requests.back().lock())
			{
				io_service().post([s, b = std::move(b)]() mutable {s->body(std::move(b));});
			}
		};

		auto tcb = [this](dstring&& k, dstring&& v)
		{
			if(requests.empty()) return;
			if(auto s = requests.back().lock())
			{
				io_service().post(
							[s, k = std::move(k), v = std::move(v)]() mutable { s->trailer(std::move(k), std::move(v)); }
				);
			}
		};

		auto ccb = [this]()
		{
			if(requests.empty()) return;
			if(auto s = requests.back().lock())
			{
				io_service().post([s]() { s->finished(); });
			}
		};

		auto fcb = [this](int error,bool&)
		{
			//failure always gets called after start...
			LOGTRACE("Codec failure handling");
			if(auto s = requests.back().lock())
			{
				io_service().post([s]() { s->error(http::error_code::success); }); //fix
			}
			if(auto s = responses.back().lock()) {
				io_service().post([s]() { s->error(http::error_code::success); });
			}
			requests.pop_front();
			responses.pop_front();
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

	bool should_stop() const noexcept override
	{
		auto finished = responses.empty();
		return error_happened || (finished && !persistent);
	}

	bool on_read(const char* data, size_t len) override
	{
		LOGTRACE(this, " Received chunk of size:", len);
		auto rv = decoder.decode(data, len);
		return rv;
	}

	bool on_write(dstring& data) override
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

	void on_error(const int&) override
	{
		LOGTRACE(this, " on_error");
		error_happened = true;
		//propagate error.
	}

	~handler_http1() override = default;

private:

	void close() override {
		if(auto s = connector()) s->close(); 
	}
	/***/
	boost::asio::io_service& io_service()
	{
		return connector()->io_service();
	}

	void do_write() override
	{
		if(connector())
			connector()->do_write();
		else
			LOGERROR("handler_factory ", this, " connector already destroyed");
	}

	void on_connector_nulled() override
	{
		error_code_distruction = INTERNAL_ERROR_LONG(408);
		/** Send back the error, to everybody */
		http::connection_error err{http::error_code::closed_by_client};
		error(err);
		//should notify everybody in the connection of the error!
		for(auto &req: requests)
		{
			if(auto s = req.lock()) io_service().post([s, err](){s->error(err);});
		}
		for(auto &res: responses)
		{
			if(auto s = res.lock()) io_service().post([s, err](){s->error(err);});
		}
		deinit();
		requests.clear();
		responses.clear();
	}

	/** Method used by responses to notify availability of new content*/
	void notify_response()
	{
		while(!responses.empty() && !requests.empty())
		{
			//replace all this with a polling mechanism!
			auto &c = responses.front();
			if(auto s = c.lock())
			{
				if(poll_response(s))
				{
					//we pop them both.
					responses.pop_front();
					requests.pop_front(); }
				else break;
			} else {
				responses.pop_front();
				requests.pop_front();
			}
		}
	}

	void set_timeout(std::chrono::milliseconds ms)
	{
		auto s = connector();
		if(s) s->set_timeout(std::move(ms));
	}

	/** Method used to retrieeve new content from a response */
	bool poll_response(std::shared_ptr<http::response> res)
	{
		auto state = res->get_state();
		while(state != http::response::state::pending) {
			switch(state) {
			case http::response::state::headers_received:
				notify_response_headers(res->get_headers());
				break;
			case http::response::state::body_received:
				notify_response_body(res->get_body());
				break;
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

	/** Response management methods. They operate on a single response*/
	void notify_response_headers(http::http_response&& res)
	{
		serialization.append(encoder.encode_header(res));
		do_write();
	}

	void notify_response_body(dstring&& b)
	{
		serialization.append(encoder.encode_body(b));
		do_write();
	}

	void notify_response_trailer(dstring&&k, dstring&&v)
	{
		serialization.append(encoder.encode_trailer(k, v));
		do_write();
	}

	void notify_response_end()
	{
		serialization.append(encoder.encode_eom());
		do_write();
	}

	/** Requests and responses currently managed by this handler*/
	std::list<std::weak_ptr<request_t>> requests;
	std::list<std::weak_ptr<response_t>> responses;


	/** Encoder for responses*/
	http::http_codec encoder;
	/** Encoder for requests */
	http::http_codec decoder;

	/** Request used by decoder to represent the received data*/
	incoming_t current_request;

	/** Dstring used to serialize the information coming from the responses*/
	dstring serialization;

	bool error_happened{false};

	errors::error_code error_code_distruction;
	http::proto_version version{http::proto_version::UNSET};

};

} //namespace

#pragma once

#include <memory>
#include "../http/http_codec.h"
#include "../http/http_structured_data.h"
#include "../log/access_record.h"
#include "../utils/log_wrapper.h"
#include "../http/server/request.h"
#include "../http/server/response.h"
#include "../connector.h"

// TODO: this and the specialization should not be here
#include "../http/server/server_traits.h"

// TODO: should not be in _server_ namespace
namespace server
{

template<typename handler_traits>
class handler_http1 final: public http_handler, public handler_traits::connection_t
{

public:
	using connection_t = typename handler_traits::connection_t;
	using remote_t = typename handler_traits::remote_t;
	using local_t = typename handler_traits::local_t;
	using local_t_object = typename std::remove_reference<decltype(((local_t*)nullptr)->get_preamble())>::type;
	std::shared_ptr<handler_http1> get_shared()
	{
		return std::static_pointer_cast<handler_http1>(this->shared_from_this());
	}

	void trigger_timeout_event() override
	{
		connection_t::timeout();
	}

	handler_http1(http::proto_version version) : version{version}
	{
		LOGINFO("HTTP1 selected");
	}

	bool start() noexcept override
	{
		connection_t::init();
		auto scb = [this](http::http_structured_data** data)
		{
			this->on_incoming_structured_data(data);
		};

		auto hcb = [this]()
		{
			if(remote_objects.empty()) return;
			if(auto s = remote_objects.back().lock())
			{
				auto keepalive = !current_decoded_object.has(http::hf_connection) ? connection_t::persistent : current_decoded_object.header(http::hf_connection) == http::hv_keepalive;
				current_decoded_object.keepalive(keepalive);
				connection_t::persistent = keepalive;
				io_service().post(
							[s, current_decoded_object = std::move(current_decoded_object)]() mutable {s->headers(std::move(current_decoded_object));}
				);
				current_decoded_object = {};
			}
		};

		auto bcb = [this](dstring&& b)
		{
			if(remote_objects.empty()) return;
			if(auto s = remote_objects.back().lock())
			{
				io_service().post([s, b = std::move(b)]() mutable {s->body(std::move(b));});
			}
		};

		auto tcb = [this](dstring&& k, dstring&& v)
		{
			if(remote_objects.empty()) return;
			if(auto s = remote_objects.back().lock())
			{
				io_service().post(
							[s, k = std::move(k), v = std::move(v)]() mutable { s->trailer(std::move(k), std::move(v)); }
				);
			}
		};

		auto ccb = [this]()
		{
			if(remote_objects.empty()) return;
			if(auto s = remote_objects.back().lock())
			{
				io_service().post([s]() { s->finished(); });
			}
		};

		auto fcb = [this](int error,bool&)
		{
			//failure always gets called after start...
			LOGTRACE("Codec failure handling");
			if(auto s = remote_objects.back().lock())
			{
				io_service().post([s]() { s->error(http::error_code::success); }); //fix
			}
			if(auto s = local_objects.back().lock()) {
				io_service().post([s]() { s->error(http::error_code::success); });
			}
			remote_objects.pop_front();
			local_objects.pop_front();
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
		auto finished = local_objects.empty();
		return error_happened || (finished && !connection_t::persistent);
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

	void on_incoming_structured_data(http::http_structured_data**);

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
		connection_t::error(err);
		//should notify everybody in the connection of the error!
		for(auto &req: remote_objects)
		{
			if(auto s = req.lock()) io_service().post([s, err](){s->error(err);});
		}
		for(auto &res: local_objects)
		{
			if(auto s = res.lock()) io_service().post([s, err](){s->error(err);});
		}
		connection_t::deinit();
		remote_objects.clear();
		local_objects.clear();
	}

	/** Method used by responses to notify availability of new content*/
	void notify_local_content()
	{
		while(!local_objects.empty() && !remote_objects.empty())
		{
			//replace all this with a polling mechanism!
			auto &c = local_objects.front();
			if(auto s = c.lock())
			{
				if(poll_local(s))
				{
					//we pop them both.
					local_objects.pop_front();
					remote_objects.pop_front(); }
				else break;
			} else {
				local_objects.pop_front();
				remote_objects.pop_front();
			}
		}
	}

	void set_timeout(std::chrono::milliseconds ms)
	{
		auto s = connector();
		if(s) s->set_timeout(std::move(ms));
	}

	/** Method used to retrieeve new content from a response */
	bool poll_local(std::shared_ptr<local_t> res)
	{
		auto state = res->get_state();
		while(state != local_t::state::pending) {
			switch(state) {
			case local_t::state::headers_received:
				notify_local_headers(res->get_preamble());
				break;
			case local_t::state::body_received:
				notify_local_body(res->get_body());
				break;
			case local_t::state::trailer_received:
			{
				auto trailer = res->get_trailer();
				notify_local_trailer(std::move(trailer.first), std::move(trailer.second));
				break;
			}
			case local_t::state::ended:
				notify_local_end();
				return true;
			default: assert(0);
			}
			state = res->get_state();
		}
		return false;
	}

	/** Local Object management methods. They operate on a single local object*/
	void notify_local_headers(local_t_object &&loc)
	{
		if(loc.has(http::hf_connection))
		{
			connection_t::persistent = loc.header(http::hf_connection) == http::hv_keepalive;
		}
		serialization.append(encoder.encode_header(loc));
		do_write();
	}

	void notify_local_body(dstring &&b)
	{
		serialization.append(encoder.encode_body(b));
		do_write();
	}

	void notify_local_trailer(dstring &&k, dstring &&v)
	{
		serialization.append(encoder.encode_trailer(k, v));
		do_write();
	}

	void notify_local_end()
	{
		serialization.append(encoder.encode_eom());
		do_write();
	}

	/** Requests and responses currently managed by this handler*/
	std::list<std::weak_ptr<remote_t>> remote_objects;
	std::list<std::weak_ptr<local_t>> local_objects;


	/** Encoder for local objects*/
	http::http_codec encoder;
	/** Encoder for remote objects */
	http::http_codec decoder;

	/** Request used by decoder to represent the received data*/
	typename std::remove_reference<decltype(((remote_t*)nullptr)->preamble())>::type current_decoded_object;

	/** Dstring used to serialize the information coming from the local object*/
	dstring serialization;

	bool error_happened{false};

	errors::error_code error_code_distruction;
	http::proto_version version{http::proto_version::UNSET};

};

template<>
inline
void handler_http1<http::server_traits>::on_incoming_structured_data(http::http_structured_data** data)
{
	auto req = std::make_shared<remote_t>(this->get_shared());
	req->init();
	auto res = std::make_shared<local_t>([this, self = this->get_shared()](){
		notify_local_content();
	});
	*data = &current_decoded_object;
	remote_objects.push_back(req);
	local_objects.push_back(res);

	request_received(std::move(req), std::move(res));
}

} //namespace

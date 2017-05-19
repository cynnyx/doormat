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
		io_service().post([this](){connection_t::timeout(); });
	}

	handler_http1(http::proto_version version) : version{version}
    {}


	std::pair<std::shared_ptr<remote_t>, std::shared_ptr<local_t>> get_user_handlers() override
	{
		auto rem = std::make_shared<remote_t>(this->get_shared());
		auto loc = std::make_shared<local_t>([this, self = this->get_shared()](){
			notify_local_content();

		});
		remote_objects.push_back(rem);
		local_objects.push_back(loc);
		return std::make_pair(rem, loc);
	}

	std::shared_ptr<remote_t> get_current() {
		if(decoding_error) return nullptr;
		if(!remote_objects.size()) {
			decoding_error = true;
			connection_t::error(http::error_code::invalid_read);
			return nullptr;
		}
		auto current_remote = remote_objects.front();
		if(current_remote->ended())
		{
			connection_t::error(http::error_code::invalid_read);
			return nullptr;
		}
		return current_remote;
	}

	bool start() noexcept override
	{
		connection_t::init();
		auto scb = [this](http::http_structured_data** data)
		{
			decoding_error = false;
			*data = &current_decoded_object;
			decoder_begin();
		};

		auto hcb = [this]()
		{
			auto current_remote = get_current();
			if(!current_remote) return;
			bool keepalive =
						(!current_decoded_object.has(http::hf_connection)) ?
							connection_t::persistent :
							current_decoded_object.header(http::hf_connection) == http::hv_keepalive;

			current_decoded_object.keepalive(keepalive);

			connection_t::persistent = keepalive;

			io_service().post(
					[s = current_remote, current_decoded_object = std::move(current_decoded_object)]() mutable
					{
						s->headers(std::move(current_decoded_object));
					}
			);

			current_decoded_object = {};

		};

		auto bcb = [this](dstring&& b)
		{

			auto current_remote = get_current();
			if(!current_remote) return;

			io_service().post([s = current_remote, b = std::move(b)]() mutable
			                  {
				                  s->body(std::move(b));
			                  });

		};

		auto tcb = [this](dstring&& k, dstring&& v)
		{

			auto current_remote = get_current();
			if(!current_remote) return;
			io_service().post([s = current_remote, k = std::move(k), v = std::move(v)]() mutable
							{
								s->trailer(std::move(k), std::move(v));
							});

		};

		auto ccb = [this]()
		{
			auto current_remote = get_current();
			if(!current_remote) return;
			io_service().post([current_remote = std::move(current_remote)]() { current_remote->finished(); });
			remote_objects.pop_front(); //finished, hence we remove the current decoded remote object from the queue.
		};

		auto ecb = [this](int error,bool&)
		{
			//failure always gets called after start...
			auto current_remote = get_current();
			if(!current_remote) return;

			io_service().post([s = std::move(current_remote)]() { s->error(http::error_code::decoding); }); //fix
			remote_objects.pop_front();
			if(auto s = local_objects.back().lock())
			{
				io_service().post([s]() { s->error(http::error_code::decoding); });
			}
			local_objects.pop_front();
		};

		decoder.register_callback(std::move(scb), std::move(hcb), std::move(bcb), std::move(tcb), std::move(ccb), std::move(ecb));
		return true;
	}

	bool should_stop() const noexcept override
	{
		auto finished = local_objects.empty();
		return finished && !connection_t::persistent;
	}

	bool on_read(const char* data, size_t len) override
	{
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


	~handler_http1() override = default;

private:

	/** Event emitted when the decoder starts; default behaviour. See specializations.*/
	void decoder_begin(){}

	void notify_all(http::error_code err) {

		for(auto current_remote: remote_objects) {
			current_remote->error(err);
		}
		for(auto &res: local_objects)
		{
			if(auto s = res.lock()) s->error(err);
		}
		remote_objects.clear();
		local_objects.clear();
	}


	/** Explicit user-requested close*/
	void close() override
	{
		user_close = true;
		if(auto s = connector()) s->close();
		notify_all(http::error_code::connection_closed);
		connection_t::deinit();
	}

	/** Handler to the io_service to post deferred callbacks*/
	boost::asio::io_service& io_service()
	{
		return connector()->io_service(); //hmmm :D
	}

	/** Requires a write if a connector is still available. */
	void do_write() override
	{
		if(connector())
			connector()->do_write();
	}

	/** Reaction to connection close*/
	void on_connector_nulled() override
	{
		if(user_close) return;
		/** Send back the error, to everybody */
		http::connection_error err{http::error_code::closed_by_client};
		notify_all(err.errc());
		connection_t::error(err);
		//should notify everybody in the connection of the error!
		connection_t::deinit();
	}

	/** Method used by responses to notify availability of new content*/
	void notify_local_content()
	{
		while(!local_objects.empty())
		{
			//replace all this with a polling mechanism!
			auto &c = local_objects.front();
			if(auto s = c.lock())
			{
				if(poll_local(s))
				{
					local_objects.pop_front();
				}
				else break;
			} else
			{
				local_objects.pop_front();
			}
		}
	}
	/** propagates the timeout request to the connector. */
	void set_timeout(std::chrono::milliseconds ms)
	{
		auto s = connector();
		if(s) s->set_timeout(std::move(ms));
	}

	/** Method used to retrieeve new content from a response */
	bool poll_local(std::shared_ptr<local_t> loc)
	{
		auto state = loc->get_state();
		while(state != local_t::state::pending)
		{
			switch(state) {
			case local_t::state::headers_received:
				notify_local_headers(loc->get_preamble());
				break;
			case local_t::state::body_received:
				notify_local_body(loc->get_body());
				break;
			case local_t::state::trailer_received:
			{
				auto trailer = loc->get_trailer();
				notify_local_trailer(std::move(trailer.first), std::move(trailer.second));
				break;
			}
			case local_t::state::ended:
				notify_local_end();
				//delaying this is very important; otherwise the client could send another request while the state is wrong.
				io_service().post([loc](){loc->cleared();});
				connection_t::cleared();
				return true;
			default: assert(0);
			}
			state = loc->get_state();
		}
		return false;
	}

	/** Local Object management methods. They operate on a single local object. */
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

	/** local objects currently being managed by the handler*/
	std::list<std::weak_ptr<local_t>> local_objects;

	/** remote objects currently awaiting for reception*/
	std::list<std::shared_ptr<remote_t>> remote_objects; 
	/** Encoder for local objects*/
	http::http_codec encoder;
	/** Encoder for remote objects */
	http::http_codec decoder;

	/** Request used by decoder to represent the received data*/
	typename std::remove_reference<decltype(((remote_t*)nullptr)->preamble())>::type current_decoded_object;

	/** Dstring used to serialize the information coming from the local object*/
	dstring serialization;
	/** User close is set to true when an explicit connection close is required by the user, avoiding sending an error*/
	bool user_close{false};
	bool decoding_error{false};
	/** Current protocol version.*/
	http::proto_version version{http::proto_version::UNSET};

};

/** Specialization providing user feedback. */
template<>
inline
void handler_http1<http::server_traits>::decoder_begin()
{
	auto f = get_user_handlers();
	user_feedback(std::move(f.first), std::move(f.second));
}



} //namespace

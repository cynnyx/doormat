#pragma once

#include <memory>
#include "../http/http_codec.h"
#include "../http/http_structured_data.h"
#include "../log/access_record.h"
#include "../utils/log_wrapper.h"
#include "../http/server/request.h"
#include "../http/server/response.h"
#include "../http/client/request.h"
#include "../http/client/response.h"
#include "../http/client/client_connection.h"
#include "../connector.h"

// TODO: these and the specialization should not be here
#include "../http/server/server_traits.h"
#include "../http/client/client_traits.h"

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

	handler_http1() = default;


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
		auto scb = [this](http::http_structured_data** data) { decoding_start(data); };
		auto hcb = [this]()	{ decoded_headers(); };
		auto bcb = [this](dstring&& b) { decoded_body(std::move(b)); };
		auto tcb = [this](dstring&& k, dstring&& v) { decoded_trailer(std::move(k), std::move(v));};
		auto ccb = [this]() {decoding_end(); };
		auto ecb = [this](int error,bool&) {decoding_failure();};

		decoder.register_callback(std::move(scb), std::move(hcb), std::move(bcb), std::move(tcb), std::move(ccb), std::move(ecb));
		return true;
	}

	void decoding_failure()
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
	}

	void decoding_end()
	{
		auto current_remote = get_current();
		if(!current_remote) return;
		io_service().post([current_remote = std::move(current_remote)]() { current_remote->finished(); });
		remote_objects.pop_front(); //finished, hence we remove the current decoded remote object from the queue.
	}

	void decoded_trailer(dstring &&k, dstring &&v)
	{
		auto current_remote = get_current();
		if(!current_remote) return;
		if(managing_continue) return;
		io_service().post([s = current_remote, k = ::std::move(k), v = ::std::move(v)]() mutable
							{
								s->trailer(std::move(k), std::move(v));
							});
	}

	void decoded_body(dstring &&b)
	{
		auto current_remote = get_current();
		if(!current_remote) return;

		io_service().post([s = current_remote, b = ::std::move(b)]() mutable
			                  {
				                  s->body(std::move(b));
			                  });
	}

	void update_persistent()
	{
		if(current_decoded_object.protocol_version() == http::proto_version::HTTP10)
		{
			connection_t::persistent = connection_t::persistent &&
					(current_decoded_object.has(http::hf_connection) &&
					 current_decoded_object.header(http::hf_connection) == http::hv_keepalive);
		} else
		{
			connection_t::persistent = connection_t::persistent &&
					(!current_decoded_object.has(http::hf_connection) ||
					 current_decoded_object.header(http::hf_connection) == http::hv_keepalive);
		}

		current_decoded_object.keepalive(connection_t::persistent);
	}

	void decoded_headers()
	{
		auto current_remote = get_current();
		if(!current_remote) return;
		update_persistent();
		io_service().post(
				[s = current_remote, current_decoded_object = ::std::move(current_decoded_object)]() mutable
					{
						s->headers(::std::move(current_decoded_object));
					}
			);

		current_decoded_object = {};
	}

	void decoding_start(http::http_structured_data **data)
	{
		decoding_error = false;
		managing_continue = false;
		*data = &current_decoded_object;
		decoder_begin();
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


	/** Explicit user-requested close*/
	void close() override
	{
		user_close = true;
		if(auto s = connector()) s->close();
		notify_all(http::error_code::connection_closed);
		connection_t::deinit();
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


	/** Handler to the io_service to post deferred callbacks*/
	boost::asio::io_service& io_service()
	{
		return connector()->io_service(); //hmmm :D
	}

	/** Requires a write if a connector is still available. */
	void do_write() override
	{
		auto c = connector();
		if(c)
			c->do_write();
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
		pending_clear_callbacks.clear();
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
				http::connection_error err{http::error_code::missing_response};
				connection_t::error(err);
				notify_all(http::error_code::missing_response);
				connection_t::deinit();
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
				pending_clear_callbacks.push_back([loc](){ loc->cleared(); });
				notify_local_end();
				connection_t::cleared();
				return true;
			default: assert(0);
			}
			state = loc->get_state();
		}
		return false;
	}


	std::vector<std::function<void()>> write_feedbacks() override {
		auto d = std::move(pending_clear_callbacks);
		pending_clear_callbacks = {};
		return d;
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

	/** List of callbacks to be called when the next write is successful. */
	std::vector<std::function<void()>> pending_clear_callbacks{};
	bool managing_continue{false};

};

/** Specialization providing user feedback. */
template<>
inline
void handler_http1<http::server_traits>::decoder_begin()
{
	auto f = get_user_handlers();
	user_feedback(std::move(f.first), std::move(f.second));
}
template<>
inline
bool handler_http1<http::server_traits>::poll_local(std::shared_ptr<http::server_traits::local_t> loc)
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
				pending_clear_callbacks.push_back([loc](){ loc->cleared(); });
				notify_local_end();
				//delaying this is very important; otherwise the client could send another request while the state is wrong.
				connection_t::cleared();
				return true;
			case local_t::state::send_continue:
			{
				http::http_response r;
				r.protocol(http::proto_version::HTTP11); //todo: make protocol parametric; fix everything.
				r.status(100);
				serialization.append(encoder.encode_header(r));
				serialization.append(encoder.encode_eom());
				do_write();
				return false;
			}
			default: assert(0);

		}
		state = loc->get_state();
	}
	return false;
}

template<>
inline
void handler_http1<http::client_traits>::decoded_headers()
{
	auto current_remote = get_current();
	if(!current_remote) return;
	update_persistent();
	managing_continue = current_decoded_object.status_code() == 100;
	if(!managing_continue)
		io_service().post(
			[s = current_remote, current_decoded_object = ::std::move(current_decoded_object)]() mutable
			{
				s->headers(::std::move(current_decoded_object));
			}
		);
	current_decoded_object = {};
}



template<>
inline
void handler_http1<http::client_traits>::decoding_end()
{
	auto current_remote = get_current();
	if(!current_remote) return;
	if(managing_continue) return io_service().post([current_remote = std::move(current_remote)]()
	    {
			current_remote->response_continue();
		});
	io_service().post([current_remote = std::move(current_remote)]() { current_remote->finished(); });
	remote_objects.pop_front(); //finished, hence we remove the current decoded remote object from the queue.
}



} //namespace

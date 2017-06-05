#pragma once

#include <memory>
#include "../http/http_codec.h"
#include "../http/http_structured_data.h"
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
	using local_t_object = typename std::remove_reference<decltype(((local_t*)nullptr)->preamble())>::type;
	using data_t = std::unique_ptr<const char[]>;

	/** \brief shared pointer to handler.
	 * 	\returns  a shared pointer to the object, with the downcasted interface.
	 * */
	std::shared_ptr<handler_http1> get_shared()
	{
		return std::static_pointer_cast<handler_http1>(this->shared_from_this());
	}

	/** \brief posts a connection timeout */
	void trigger_timeout_event() override
	{
		io_service().post([this](){connection_t::timeout(); });
	}

	handler_http1() = default;

	/** \brief returns a pair of handlers to communicate with the server
	 * \returns a pair containing a remote object and a local object. */
	std::pair<std::shared_ptr<remote_t>, std::shared_ptr<local_t>> get_user_handlers() override
	{
		auto conn = connector();
		if(!conn) return {};
		auto rem = std::make_shared<remote_t>(this->get_shared(), conn->io_service());
		auto loc = std::make_shared<local_t>([this, self = this->get_shared()](){
			notify_local_content();

		}, conn->io_service());
		remote_objects.push_back(rem);
		local_objects.push_back(loc);
		return std::make_pair(rem, loc);
	}

	/** \brief returns currently used remote object, while it is still in the decoding phase.
	 * \return a shared pointer to the remote object*/
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

	/** \brief starts the server.
	 * \returns true if it starts correctly; false otherwise.
	 * */
	bool start() noexcept override
	{
		connection_t::init();
		auto scb = [this](http::http_structured_data** data) { decoding_start(data); };
		auto hcb = [this]()	{ decoded_headers(); };
		auto bcb = [this](dstring chunk)
		{
			// TODO: we need to remove dstrings from http_codec
			auto ptr = std::make_unique<char[]>(chunk.size());
			std::copy(chunk.cbegin(), chunk.cend(), ptr.get());
			decoded_body(std::move(ptr), chunk.size());
		};
		auto tcb = [this](std::string&& k, std::string&& v) { decoded_trailer(std::move(k), std::move(v));};
		auto ccb = [this]() {decoding_end(); };
		auto ecb = [this](int error,bool&) {decoding_failure();};

		decoder.register_callback(std::move(scb), std::move(hcb), std::move(bcb), std::move(tcb), std::move(ccb), std::move(ecb));
		return true;
	}

	/** \brief decoding failure management
	 * */
	void decoding_failure()
	{
		//failure always gets called after start...
		auto current_remote = get_current();
		if(!current_remote) return;

		current_remote->error(http::error_code::decoding);
		remote_objects.pop_front();
		if(auto s = local_objects.back().lock())
			 s->error(http::error_code::decoding);
		local_objects.pop_front();
	}
	/** \brief decoding end management
	 * */
	void decoding_end()
	{
		auto current_remote = get_current();
		if(!current_remote) return;
		current_remote->finished();
		remote_objects.pop_front(); //finished, hence we remove the current decoded remote object from the queue.
	}
	/** \brief trailer decoding management
	 *	\param k the key of the trailer
	 *	\param v the value of the trailer
	 * */
	void decoded_trailer(std::string&& k, std::string&& v)
	{
		auto current_remote = get_current();
		if(!current_remote) return;
		if(managing_continue) return;
		current_remote->trailer(std::move(k), std::move(v));
	}

	/** \brief body decoding management
	 *	\param b the decoded body content
	 *	\param size the size of the content
	 * */
	void decoded_body(data_t b, size_t size)
	{
		auto current_remote = get_current();
		if(!current_remote) return;
		current_remote->body(std::move(b), size);
	}


	/** \brief changes the persistency state of the handler depending on the content of the messages passing into it.
	 *
	 * */
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

	/** \brief management of headers decoding
	 *
	 * */
	void decoded_headers()
	{
		auto current_remote = get_current();
		if(!current_remote) return;
		update_persistent();
		current_remote->headers(::std::move(current_decoded_object));
		current_decoded_object = {};
	}

	/** \brief starts decoding.
	 * */
	void decoding_start(http::http_structured_data **data)
	{
		decoding_error = false;
		managing_continue = false;
		*data = &current_decoded_object;
		decoder_begin();
	}
	/** \brief checks whether the handler (according to the protocol and the managed resources) should stop
	 * \return true if it should stop; false otherwise.
	 * */
	bool should_stop() const noexcept override
	{
		auto finished = local_objects.empty();
		return finished && !connection_t::persistent;
	}

	/** \brief actions carried on when a read has been performed
	 * \param data the readed data
	 * \param len the size of the data readed
	 * \return true in case decoding was successful; false otherwise.
	 * */
	bool on_read(const char* data, size_t len) override
	{
		auto rv = decoder.decode(data, len);
		return rv;
	}

	/** \brief returns data to be written on the connector.
	 * \param data reference to the array in which the data will be placed.
	 * \return true in case a write should be really performed.
	 * */
	bool on_write(std::string& data) override
	{
		if(connector())
		{
			if(!serialization.empty())
			{
				data = std::move(serialization);
				serialization = {};
				return true;
			}
		}
		return false;
	}


	/** \brief explicit user-requested close
	 * */
	void close() override
	{
		user_close = true;
		if(auto s = connector()) s->close();
		notify_all(http::error_code::connection_closed);
		connection_t::deinit();
	}


	~handler_http1() override = default;

private:

	/** \brief event emitted when the decoder starts; default behaviour does nothing. See specializations.*/
	void decoder_begin(){}

	/** \brief notifies an error to all currently managed objects.
	 * \param err the error code. */
	void notify_all(http::error_code err) {

		for(auto current_remote: remote_objects) {
			current_remote->error(err);
		}
		for(auto &res: local_objects)
		{
			if(auto s = res.lock())
			{
				s->error(err);
			}
		}
		remote_objects.clear();
		local_objects.clear();
	}


	/** \brief handler to the io_service to post deferred callbacks
	 * \returns a reference to the io_service
	 * */
	boost::asio::io_service& io_service()
	{
		return connector()->io_service(); //hmmm :D
	}

	/** \brief requires a write if a connector is still available. */
	void do_write() override
	{
		auto c = connector();
		if(c)
			c->do_write();
	}

	/** \brief reaction to connection close*/
	void on_connector_nulled() override
	{
		if(user_close) return;
		/** Send back the error, to everybody */
		http::connection_error err{http::error_code::closed_by_client};
		notify_all(err.errc());
		connection_t::error(err);
		//should notify everybody in the connection of the error!
		connection_t::deinit();
		for(auto &cb : pending_clear_callbacks) {
			cb.second();
		}
		pending_clear_callbacks.clear();
	}

	/** \brief method used by responses to notify availability of new content
	 * */
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
				std::cout << "bad error: missing stream element" << std::endl; 
				http::connection_error err{http::error_code::missing_stream_element};
				connection_t::error(err);
				notify_all(http::error_code::missing_stream_element);
				connection_t::deinit();
			}
		}
	}
	/** \brief propagates the timeout request to the connector.
	 * \param ms the timeout interval requested.
	 * */
	void set_timeout(std::chrono::milliseconds ms) override
	{
		auto s = connector();
		if(s) s->set_timeout(std::move(ms));
	}

	/** \brief retrieve new content from a remote object
	 * \returns true if the remote object has finished and hence can be removed.
	 * */
	bool poll_local(std::shared_ptr<local_t> loc)
	{
		auto state = loc->get_state();
		while(state != local_t::state::pending)
		{
			switch(state) {
			case local_t::state::headers_received:
				notify_local_headers(loc->preamble());
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
				pending_clear_callbacks.emplace_back([loc](){ loc->cleared(); }, [loc](){loc->error(http::error_code::missing_stream_element); });
				notify_local_end();
				connection_t::cleared();
				return true;
			default: assert(0);
			}
			state = loc->get_state();
		}
		return false;
	}

	/** \brief gets callbacks to be executed once the subsequent write has been completed successfully
	 * \returns vector of pairs of <success, error> callback to be called when write has been successfully performed.*/
	std::vector<std::pair<std::function<void()>, std::function<void()>>> write_feedbacks() override {
		auto d = std::move(pending_clear_callbacks);
		pending_clear_callbacks = {};
		return d;
	}

	/** \brief Local Object management method for headers*/
	void notify_local_headers(local_t_object &&loc)
	{
		if(loc.has(http::hf_connection))
		{
			connection_t::persistent = loc.header(http::hf_connection) == http::hv_keepalive;
		}
		serialization.append(encoder.encode_header(loc));
		do_write();
	}

	/** \brief Local Object management method for body*/
	void notify_local_body(std::string&& body)
	{
		serialization.append(encoder.encode_body(std::move(body)));
		do_write();
	}

	/** \brief Local Object management method for trailers*/
	void notify_local_trailer(std::string&& k, std::string&& v)
	{

		serialization.append(encoder.encode_trailer(k, v));
		do_write();
	}
	/** \brief Local Object management method for end*/
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
	std::vector<std::pair<std::function<void()>, std::function<void()>>> pending_clear_callbacks{};
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
				notify_local_headers(loc->preamble());
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
				//todo: avoid hardcoded error in this function; it is just a very, very bad idea.
				pending_clear_callbacks.emplace_back([loc](){ loc->cleared(); }, [loc](){loc->error(http::error_code::missing_stream_element); });
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
		current_remote->headers(::std::move(current_decoded_object));
	current_decoded_object = {};
}



template<>
inline
void handler_http1<http::client_traits>::decoding_end()
{
	auto current_remote = get_current();
	if(!current_remote) return;
	remote_objects.pop_front();
	if(managing_continue)
		return current_remote->response_continue();

	current_remote->finished();
}



} //namespace

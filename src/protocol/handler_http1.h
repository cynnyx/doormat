#pragma once

#include <memory>
#include "../http/http_codec.h"
#include "../http/http_structured_data.h"
#include "../chain_of_responsibility/node_erased.h"
#include "../log/access_record.h"

#include "handler_factory.h"
#include "../utils/log_wrapper.h"
#include "../chain_of_responsibility/node_interface.h"
#include "../chain_of_responsibility/chain_of_responsibility.h"
#include "../chain_of_responsibility/callback_initializer.h"

namespace server
{

/**
 * @brief The handler_http1 class

 This is the handler reponsible to receive http/1.1
 request from the client, parse them and
 forward'em down our filter chain towards the boards.

 An instance of this class is created for every request.

 Once request have been fully processed and
 control is returned from the above chain it should
 encode the response accordingly and
 ship it to the client via the underlying tcp_connector;
*/

/** \brief a transaction_handler manages a single request_response transaction within a persistent http1 session*/
class handler_http1 : public handler_interface
{
	bool persistent_connection = true;
	bool error_happened = false;
	http::http_codec decoder;
	errors::error_code error_code_distruction;
	http::proto_version version{http::proto_version::UNSET};
public:
	handler_http1(http::proto_version version);

	bool start() noexcept override;
	bool should_stop() const noexcept override;
	bool on_read(const char*, size_t) override;
	bool on_write(dstring&) override;

	void on_eom() override;
	void on_error(const int&) override;
	virtual ~handler_http1() = default;
protected:
	void do_write() override;
	void on_connector_nulled() override;
private:

    dstring serialization;

    void notify_response_headers(http::http_response&& res) override
    {
        serialization.append(encoder.encode_header(res));
		do_write();
    }

	void notify_response_body(dstring&& b) override {
		serialization.append(encoder.encode_body(b));
		do_write();
	}

	void notify_response_trailer(dstring&&k, dstring&&v){
		serialization.append(encoder.encode_trailer(k, v));
		do_write();
	}

	void notify_response_end(){
		serialization.append(encoder.encode_eom());
		do_write();
	}

	bool some_message_started( http::proto_version& proto ) const noexcept;
	class transaction_handler
	{
		dstring encoded_data;
		http::http_request data;
		http::http_response continue_response;
		std::shared_ptr<handler_interface> enclosing{nullptr};
		http::http_codec encoder;
		logging::access_recorder access;
	public:
		// TODO this stuff must be private - they are public just because someone messed up the incapsulation
		bool message_started{false};
		bool message_ended{false};
		bool persistent{false};
		bool request_is_finished{false};
		std::unique_ptr<node_interface> cor;

		transaction_handler(std::shared_ptr<handler_interface> enclosing, bool ssl)
			: data{ssl}, enclosing{enclosing}
		{
			// @todo This should be refactored
			access.set_request_start();

		}

		~transaction_handler() noexcept
		{
			access.commit();
		}

		bool has_encoded_data() const noexcept
		{
			return encoded_data;
		}

		dstring get_encoded_data()
		{
			dstring tmp;
			std::swap(encoded_data, tmp);
			return tmp;
		}

		http::http_request& get_data() noexcept
		{
			return data;
		}

		void on_request_preamble(http::http_request&& message);
		void on_request_body(dstring&& c) { access.append_response_body(c); cor->on_request_body(std::move(c)); }
		void on_request_finished() { request_is_finished = true; cor->on_request_finished(); }
		void on_request_trailer(dstring&& k, dstring&& v) 
		{ 
			access.append_request_trailer( k, v ); 
			cor->on_request_trailer( std::move(k), std::move(v) ); 
		}
		void on_header(http::http_response &&);
		void on_body(dstring&&);
		void on_trailer(dstring&&, dstring&&);
		void on_eom();
		void on_error(const int &);
		void on_response_continue();
		void on_request_canceled(const errors::error_code &ec);
		bool disposable() const noexcept { return encoded_data.empty() && message_ended && request_is_finished; }
	};

	std::list<transaction_handler> th;
	http::http_codec encoder;

};

} //namespace

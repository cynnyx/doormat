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
 *
 * */
class handler_http1 final: public http_handler
{

public:
	handler_http1(http::proto_version version);

	bool start() noexcept override;
	bool should_stop() const noexcept override;
	bool on_read(const char*, size_t) override;
	bool on_write(dstring&) override;
	void on_error(const int&) override;

	~handler_http1() override = default;

private:
	void do_write() override;
	void on_connector_nulled() override;

	/** Method used by responses to notify availability of new content*/
    void notify_response();
    /** Method used to retrieeve new content from a response */
    bool poll_response(std::shared_ptr<http::response>);

    /** Response management methods. They operate on a single response*/
    void notify_response_headers(http::http_response&& res);
    void notify_response_body(dstring&& b);
    void notify_response_trailer(dstring&&k, dstring&&v);
    void notify_response_end();

    /** Requests and responses currently managed by this handler*/
	std::queue<std::weak_ptr<http::request>> requests;
    std::queue<std::weak_ptr<http::response>> responses;

    /** Encoder for responses*/
	http::http_codec encoder;
    /** Encoder for requests */
    http::http_codec decoder;

    /** Request used by decoder to represent the received data*/
    http::http_request current_request;

    /** Dstring used to serialize the information coming from the responses*/
    dstring serialization;

	bool error_happened{false};

    errors::error_code error_code_distruction;

    http::proto_version version{http::proto_version::UNSET};


};

} //namespace

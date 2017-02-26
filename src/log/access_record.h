#ifndef DOORMAT_ACCESS_RECORD_H_
#define DOORMAT_ACCESS_RECORD_H_

#include "../utils/doormat_types.h"
#include "../http/http_response.h"
#include "../http/http_request.h"

#include <chrono>
#include <string>

namespace logging
{

class inspector_log;

struct access_record
{
	using clock = std::chrono::steady_clock;

	dstring remote_addr;
	dstring remote_user;
	dstring req;
	uint16_t status{0};
	size_t body_bytes_sent{0};
	dstring referer;
	dstring user_agent;
	clock::time_point start;
	clock::time_point end;
	bool pipe{false};
	size_t req_length{0};
	dstring x_debug_cyn_ip;
	dstring host;
};

class access_recorder final
{
	friend class inspector_log;
	using clock = access_record::clock;
	
	access_record ar_;
	dstring method;
	dstring urihost;
	dstring port;
	dstring query;
	dstring fragment;
	dstring protocol;
	dstring schema;
	std::multimap<dstring,dstring> req_headers;
	std::list<dstring> req_body;
	std::multimap<dstring,dstring> req_trailers;
	std::multimap<dstring,dstring> resp_headers;
	std::list<dstring> resp_body;
	std::multimap<dstring,dstring> resp_trailers;
	
	std::size_t req_len{0};
	std::size_t req_body_len{0};
	std::size_t resp_len{0};
	std::size_t resp_body_len{0};
	bool committed_{false};
	int err_{0};
	bool cancelled_{false};
	bool continued_{false};
public:
	access_recorder() = default;
	access_recorder(const access_recorder&);
	access_recorder(access_recorder&&) = default;

	access_recorder& operator=(const access_recorder&) = default;
	access_recorder& operator=(access_recorder&&) = default;

	void set_request_start();
	void set_request_end();
	void set_pipe(bool pipe) noexcept;

	void error( int err ) noexcept;
	void cancel() noexcept;
	void continued() noexcept;
	void commit() noexcept;
	void request( const http::http_request& r );
	void response( const http::http_response& r );
	void append_response_body( const dstring& body );
	void append_response_trailer( const dstring& k, const dstring& v );
	void append_request_body( const dstring& body );
	void append_request_trailer( const dstring& k, const dstring& v );
	void add_request_size( std::size_t s ) noexcept { ar_.req_length += s; }

	const access_record& access() const noexcept { return ar_; }
};

}

#endif

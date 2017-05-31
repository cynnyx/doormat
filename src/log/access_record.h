#ifndef DOORMAT_ACCESS_RECORD_H_
#define DOORMAT_ACCESS_RECORD_H_

#include "../utils/doormat_types.h"
#include "../http/http_response.h"
#include "../http/http_request.h"

#include <chrono>
#include <string>

namespace stats
{
class stats_manager;
}

namespace logging
{

class inspector_log;
class access_log;

struct access_record
{
	using clock = std::chrono::steady_clock;

	std::string remote_addr;
	std::string remote_user;
	std::string req;
	uint16_t status{0};
	size_t body_bytes_sent{0};
	std::string referer;
	std::string user_agent;
	clock::time_point start;
	clock::time_point end;
	bool pipe{false};
	size_t req_length{0};
	std::string x_debug_cyn_ip;
	std::string host;
};

class access_recorder final
{
	friend class inspector_log;
	using clock = access_record::clock;
	
	access_log& access_log_;
	inspector_log& inspector_log_;
	stats::stats_manager& stats_manager_;
	access_record ar_;
	std::string method;
	std::string urihost;
	std::string port;
	std::string query;
	std::string fragment;
	std::string protocol;
	std::string schema;
	std::multimap<std::string, std::string> req_headers;
	std::list<std::string> req_body;
	std::multimap<std::string, std::string> req_trailers;
	std::multimap<std::string, std::string> resp_headers;
	std::list<std::string> resp_body;
	std::multimap<std::string, std::string> resp_trailers;
	
	std::size_t req_len{0};
	std::size_t req_body_len{0};
	std::size_t resp_len{0};
	std::size_t resp_body_len{0};
	bool committed_{false};
	int err_{0};
	bool cancelled_{false};
	bool continued_{false};
public:
	access_recorder(access_log& al, inspector_log& il, stats::stats_manager& sm);
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
	void append_response_body( const std::string& body );
	void append_response_trailer( const std::string& k, const std::string& v );
	void append_request_body( const std::string& body );
	void append_request_trailer(const std::string& k, const std::string& v );
	void add_request_size( std::size_t s ) noexcept { ar_.req_length += s; }

	const access_record& access() const noexcept { return ar_; }
};

}

#endif

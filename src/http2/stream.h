#ifndef STREAM__H
#define STREAM__H

#include "../../deps/nghttp2/build/include/nghttp2/nghttp2.h"

#include <cstdint>
#include <cstddef>
#include <string>
#include <memory>

#include "session.h"
#include "../http/http_structured_data.h"
#include "../http/http_request.h"
#include "../protocol/http_handler.h"

namespace http2
{
	
class stream final
{
	std::int32_t id_;
	std::int32_t status; 
	bool headers_sent{false};
	bool body_sent{false};
	bool resume_needed_{false};
	bool eof_{false};
	bool errored{false};
	bool closed_{false};
	std::list<dstring> body;
	std::size_t body_index{0};
	nghttp2_nv* nva{nullptr}; // headers HTTP2
	std::size_t nvlen{0};
	http::http_structured_data::headers_map trailers;
	nghttp2_nv* trailers_nva{nullptr};
	std::size_t trailers_nvlen{0};

	std::shared_ptr<session> s_owner{nullptr};
	http::http_structured_data::headers_map prepared_headers;
	http::http_request request{};
	
	nghttp2_data_provider prd;
	
	// Output - when the library wants to read our buffer to send everything to the client.
	static ssize_t data_source_read_callback ( nghttp2_session *session, std::int32_t stream_id, std::uint8_t *buf, 
		std::size_t length, std::uint32_t *data_flags, nghttp2_data_source *source, void *user_data );
	
	void destroy_headers( nghttp2_nv** h ) noexcept;
	void create_headers( nghttp2_nv** h ) noexcept;
	bool has_trailers() const noexcept { return trailers.size() > 0; }
	bool defer() const noexcept;
	bool body_empty() const noexcept;
	std::size_t body_length() const noexcept;
	bool is_last_frame( std::size_t length ) const noexcept;
	bool body_eof() const noexcept;
	bool _headers_sent{false};
	std::function<void(stream*, session*)> destructor;
public:

	using data_t = std::unique_ptr<const char[]>;

//	stream( std::function<void(stream*, session*)> des );
	stream(std::shared_ptr<server::http_handler> s, std::function<void(stream*, session*)> des, std::int16_t prio = 0 );
	stream( const stream& ) = delete;
	stream& operator=( const stream& ) = delete;
	stream( stream&& o ) noexcept;
	stream& operator=( stream&& o ) noexcept;

	//fixme
	void path( const std::string& p ) { request.path( p ); }
	std::string path() const { return request.path(); }
	void method( const std::string& p ) { request.method( p ); }
	void uri_host( const std::string& p ) noexcept;
	void scheme( const std::string& p ) noexcept { request.schema( p ); }
	void add_header(std::string key, std::string value);
	void query( const std::string& query ) { request.query( query ); }
	void fragment( const std::string& frag ) { request.fragment( frag ); }
	void set_handlers(std::shared_ptr<http::request> req_handler, std::shared_ptr<http::response> res_handler);

	std::shared_ptr<http::request> req;
	std::weak_ptr<http::response> res;
	bool valid() const noexcept { return id_ >= 0; }

	std::int32_t id() const noexcept { return id_; }
	void id( std::int32_t i ) noexcept { id_ = i; }

	// Request / Response management
	void on_request_header_complete();
	void on_request_body(data_t, size_t);
	void on_request_finished();
	void on_header(http::http_response &&);
	void on_body(data_t, size_t);
	void on_trailer(std::string&&, std::string&&);
	void on_eom();

	void flush() noexcept;
	void die() noexcept;
	~stream();
};

}

#endif

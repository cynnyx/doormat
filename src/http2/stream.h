#ifndef STREAM__H
#define STREAM__H

#include "../../deps/nghttp2/build/include/nghttp2/nghttp2.h"

#include <cstdint>
#include <cstddef>
#include <string>
#include <memory>

#include "../chain_of_responsibility/node_interface.h"
#include "session.h"
#include "../http/http_structured_data.h"
#include "../http/http_request.h"
#include "../log/access_record.h"

namespace http2
{

// enum class recv_state : uint8_t 
// {
// 	HEADER,
// 	CHUNK_HEADER,
// 	BODY,
// 	CHUNK_BODY,
// 	EOM,
// 	EOC,
// 	TRAILER
// }
	
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
	// TODO Used for prioritization! In the future.
	std::int32_t weight_{0};
	
	std::unique_ptr<node_interface> managed_chain{nullptr};

	std::list<dstring> body;
	std::size_t body_index{0};
	nghttp2_nv* nva{nullptr}; // headers HTTP2
	std::size_t nvlen{0};
	http::http_structured_data::headers_map trailers;
	nghttp2_nv* trailers_nva{nullptr};
	std::size_t trailers_nvlen{0};

	session* const session_{nullptr};
	
	http::http_structured_data::headers_map prepared_headers;
	http::http_request request{true};
	
	nghttp2_data_provider prd;
	
	logging::access_recorder logger;
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
	
	std::function<void(stream*, session*)> destructor;
public:
//	stream( std::function<void(stream*, session*)> des );
	stream( session* s, std::function<void(stream*, session*)> des, std::int16_t prio = 0 );
	stream( const stream& ) = delete;
	stream& operator=( const stream& ) = delete;
	stream( stream&& o ) noexcept;
	stream& operator=( stream&& o ) noexcept;
	
	void path( const dstring& p ) { request.path( p ); }	
	dstring path() const { return request.path(); }
	void method( const dstring& p ) { request.method( p ); }	
	void uri_host( const dstring& p ) noexcept;
	void scheme( const dstring& p ) noexcept { request.schema( p ); }
	void add_header( const dstring& key, const dstring& value ) { request.header( key, value ); }
	void query( const dstring& query ) { request.query( query ); }
	void fragment( const dstring& frag ) { request.fragment( frag ); }
	
	bool valid() const noexcept { return id_ >= 0; }
	void invalidate() noexcept;
	
	std::int32_t id() const noexcept { return id_; }
	void id( std::int32_t i ) noexcept { id_ = i; }
	
	std::int16_t weight() const noexcept{ return weight_; }
	void weight( std::int16_t p ) noexcept;
	
	// Request / Response management 
// 	void on_request_preamble(http::http_request&& message);
	void on_request_header( http::http_request::header_t&& h ); // NGHttp2 friendly
	void on_request_header_complete();
	void on_request_body(dstring&& c);
	// on request trailer missing?
	void on_request_finished();
	void on_request_ack();
	void on_header(http::http_response &&);
	void on_body(dstring&&);
	void on_trailer(dstring&&, dstring&&);
	void on_eom();
	void on_error(const int &);
	void on_response_continue();
	void on_request_canceled(const errors::error_code &ec);
	
	void on_write( dstring& out );
	void on_read( dstring&& in );
	void flush() noexcept;
	void closed() noexcept { closed_ = true; }
	void die() noexcept;
	~stream();
};

}

#endif

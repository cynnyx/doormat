#ifndef STREAM_CLIENT_H
#define STREAM_CLIENT_H

#include <nghttp2/nghttp2.h>

#include <cstdint>
#include <cstddef>
#include <string>
#include <memory>

#include "../http/http_request.h"
#include "../http/http_response.h"
#include "../protocol/http_handler.h"

namespace http
{
class http_request;
class client_response;
class client_request;
}

namespace http2
{

class session_client;

class stream_client final
{
	struct req_pseudo_headers
	{
		/**
		 * @brief init_path: check h2spec 8.1.2.3
		 * @param path
		 * @param method
		 * @return
		 */
		static std::string init_path(const std::string& path, const std::string& method)
		{
			if(!path.empty())
				return path;

			static const std::string options = "OPTIONS";
			return method == options ? "*" : "/";
		}

		req_pseudo_headers() = default;
		req_pseudo_headers(const http::http_request& req)
			: method{req.method()}
			, scheme{req.schema()}
			, authority{req.hostname()}
			, path{init_path(req.path(), req.method())}
		{
			if(!req.query().empty())
				path += '?' + req.query();
		}

		std::string method;
		std::string scheme;
		std::string authority;
		std::string path;
	};

	std::int32_t id_;
	std::int32_t status_{200};
	bool headers_sent{false};
	bool body_sent{false};
	bool resume_needed_{false};
	bool eof_{false};
	bool errored{false};
	bool closed_{false};
	std::list<std::string> body{};
	std::size_t body_index{0};
	nghttp2_nv* nva{nullptr}; // headers HTTP2
	std::size_t nvlen{0};
	http::http_structured_data::headers_map trailers;
	nghttp2_nv* trailers_nva{nullptr};
	std::size_t trailers_nvlen{0};

	std::shared_ptr<session_client> s_owner{nullptr};
	http::http_structured_data::headers_map prepared_headers;
	req_pseudo_headers pseudo;
	http::http_response response;

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
	std::function<void(stream_client*, session_client*)> destructor;
public:

	using data_t = std::unique_ptr<const char[]>;

	stream_client(std::shared_ptr<server::http_handler> s, std::function<void(stream_client*, session_client*)> des, std::int16_t prio = 0 );
	stream_client( const stream_client& ) = delete;
	stream_client& operator=( const stream_client& ) = delete;
	stream_client( stream_client&& o ) noexcept;
	stream_client& operator=( stream_client&& o ) noexcept;

	//fixme
//	void path( const std::string& p ) { response.path( p ); }
//	std::string path() const { return response.path(); }
//	void method( const std::string& p ) { response.method( p ); }
	void uri_host( const std::string& p ) noexcept;
	void status( int32_t s) noexcept;
//	void scheme( const std::string& p ) noexcept { response.schema( p ); }
	void add_header(std::string key, std::string value);
//	void query( const std::string& query ) { response.query( query ); }
//	void fragment( const std::string& frag ) { response.fragment( frag ); }
	void set_handlers(std::shared_ptr<http::client_request> req_handler, std::shared_ptr<http::client_response> res_handler);

	std::shared_ptr<http::client_response> res{nullptr};
	std::shared_ptr<http::client_request> req;
	bool valid() const noexcept { return id_ >= 0; }

	std::int32_t id() const noexcept { return id_; }
	void id( std::int32_t i ) noexcept { id_ = i; }

	// Request / Response management
	void on_response_header_complete();
	void on_response_body(data_t, size_t);
	void on_response_finished();
	void on_header(http::http_request &&);
	void on_body(data_t, size_t);
	void on_trailer(std::string&&, std::string&&);
	void on_eom();

	void flush() noexcept;
	void die() noexcept;
	~stream_client();

	void notify_error(http::error_code ec);
};

}

#endif

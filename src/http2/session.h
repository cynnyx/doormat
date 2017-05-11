#ifndef SESSION__H
#define SESSION__H

#include "nghttp2/nghttp2.h"
#include "../utils/doormat_types.h"
#include "../protocol/handler_factory.h"
#include "../connector.h"
#include "../log/access_record.h"
#include "../chain_of_responsibility/error_code.h"

#include <memory>
#include <vector>

namespace http2
{

class stream;

extern const std::size_t header_size_bytes;

class session : public server::handler_interface
{
	using session_deleter = std::function<void(nghttp2_session*)>;
	std::unique_ptr<nghttp2_session, session_deleter> session_data;

	static int on_frame_recv_callback ( nghttp2_session *session_,
		const nghttp2_frame *frame, void *user_data);
	static int on_stream_close_callback (nghttp2_session *session_, int32_t stream_id,
		uint32_t error_code, void *user_data );
	static int on_begin_headers_callback ( nghttp2_session *session_, const nghttp2_frame *frame,
		void *user_data );
	static int on_header_callback ( nghttp2_session *session_, const nghttp2_frame *frame,
		const uint8_t *name, size_t namelen, const uint8_t *value, size_t valuelen,
		uint8_t flags, void *user_data );
// 	static ssize_t data_source_read_callback(nghttp2_session *session_, int32_t stream_id,
// 		uint8_t *buf, size_t length, uint32_t *data_flags, nghttp2_data_source *source, void *user_data);
	static int on_data_chunk_recv_callback(nghttp2_session *session_, uint8_t flags, int32_t stream_id,
		const uint8_t *data, size_t len, void *user_data );

	static int frame_send_callback (nghttp2_session *session, const nghttp2_frame *frame, void *user_data );
	static int frame_not_send_callback ( nghttp2_session *session, const nghttp2_frame *frame,
		int lib_error_code, void *user_data );

	void send_connection_header();
	stream* create_stream( std::int32_t id );
	void go_away();

	nghttp2_mem all;
	nghttp2_option* options;
	bool gone{false};
	std::int32_t stream_counter{0};
public:
	session();

	// Connector should catch exception from here and shut down connection
	bool start() noexcept override;
	bool on_read(const char*, size_t) override;
	bool on_write( dstring& chunk ) override;
	bool should_stop() const noexcept override;

	void do_write() override;
	void on_connector_nulled() override;

	void on_eom() override;
	void on_error(const int &) override;

	void finished_stream() noexcept;
	virtual ~session() { nghttp2_option_del( options ); }

	// Push interface to come

	nghttp2_session* next_layer() noexcept { return session_data.get(); }
	nghttp2_mem* next_layer_allocator() noexcept { return &all; }
};

}

#endif

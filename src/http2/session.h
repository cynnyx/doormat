#ifndef SESSION__H
#define SESSION__H

#include "../../deps/nghttp2/build/include/nghttp2/nghttp2.h"
#include "../utils/doormat_types.h"
#include "../connector.h"
#include "../log/access_record.h"
#include "../chain_of_responsibility/error_code.h"
#include "../protocol/http_handler.h"
#include "../http/server/server_connection.h"

#include <memory>

namespace http2
{

class stream;
static constexpr const std::int32_t default_max_concurrent_streams = 100;
extern const std::size_t header_size_bytes;

class session : public server::http_handler, public http::server_connection
{
	using session_deleter = std::function<void(nghttp2_session*)>;
	std::unique_ptr<nghttp2_session, session_deleter> session_data;

	/** Callbacks used by nghttp2 to communicate events*/
	static int on_frame_recv_callback ( nghttp2_session *session_, const nghttp2_frame *frame, void *user_data);
	static int on_stream_close_callback (nghttp2_session *session_, int32_t stream_id, uint32_t error_code, 
		void *user_data );
	static int on_begin_headers_callback ( nghttp2_session *session_, const nghttp2_frame *frame, void *user_data );
	static int on_header_callback ( nghttp2_session *session_, const nghttp2_frame *frame, const uint8_t *name, 
		size_t namelen, const uint8_t *value, size_t valuelen, uint8_t flags, void *user_data );
	static int on_data_chunk_recv_callback(nghttp2_session *session_, uint8_t flags, int32_t stream_id,
		const uint8_t *data, size_t len, void *user_data );
	static int frame_send_callback (nghttp2_session *session, const nghttp2_frame *frame, void *user_data );
	static int frame_not_send_callback ( nghttp2_session *session, const nghttp2_frame *frame,
		int lib_error_code, void *user_data );

	/** Sends connection settings*/
	void send_connection_header();

	stream* create_stream( std::int32_t id );

	void go_away();

	nghttp2_mem all;
	nghttp2_option* options;
	bool gone {false};
	std::int32_t stream_counter {0};
	
	// TODO
	void set_timeout(std::chrono::milliseconds) override;

	std::pair<std::shared_ptr<http::request>, std::shared_ptr<http::response>> get_user_handlers() override;
	std::shared_ptr<session> get_shared();
	std::uint32_t max_concurrent_streams;
	std::vector<std::pair<std::function<void()>, std::function<void()>>> pending;
	std::list<stream *> listeners;
	void notify_error(http::error_code ec);
	bool user_close{false};
public:
    session(std::uint32_t max_concurrent_streams = default_max_concurrent_streams);
	virtual std::vector<std::pair<std::function<void()>, std::function<void()>>> write_feedbacks()
	{
		auto all_pending = std::move(pending);
		pending = {};
		return all_pending;
	};

	void add_pending_callbacks(std::function<void()> clear, std::function<void()> error) {
		pending.emplace_back(std::move(clear), std::move(error));
	}
	// TODO!
    void trigger_timeout_event() override;
    // Connector should catch exception from here and shut down connection
    bool start() noexcept override;
    bool on_read(const char*, size_t) override;
    bool on_write( std::string& chunk ) override;
    bool should_stop() const noexcept override;

    void do_write() override;
    void on_connector_nulled() override;

    void finished_stream() noexcept;
    virtual ~session();

    void close() override;
    // Push interface to come

    nghttp2_session* next_layer() noexcept { return session_data.get(); }
    nghttp2_mem* next_layer_allocator() noexcept { return &all; }

	void subscribe(stream *s)
	{
		assert(std::find(listeners.begin(), listeners.end(), s) == listeners.end());
		listeners.push_back(s);
	}

	void unsubscribe(stream *s)
	{
		std::cout << std::endl; // AHHHHHH 

		listeners.remove(s);
	}
};

}

#endif

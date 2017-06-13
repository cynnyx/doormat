#include <cstring>

#include "session_client.h"
#include "stream_client.h"
#include "http2error.h"
#include "../utils/log_wrapper.h"
#include "http2alloc.h"
#include "../http/client/request.h"
#include "../http/client/response.h"

namespace http2
{

session_client::session_client(std::uint32_t max_concurrent_streams):
				session_data{nullptr, [] ( nghttp2_session* s ) { if ( s ) nghttp2_session_del ( s ); }},
				max_concurrent_streams{max_concurrent_streams}
{
	LOGTRACE("Session: ", this );
	nghttp2_option_new( &options );
	nghttp2_option_set_peer_max_concurrent_streams( options, max_concurrent_streams );

	all = create_allocator();

	// TODO: add nghttp2_on_invalid_frame_recv_callback callback to end lifecycles. needed?

	nghttp2_session_callbacks *callbacks;
	nghttp2_session_callbacks_new(&callbacks);
	nghttp2_session_callbacks_set_on_frame_recv_callback(callbacks,
			on_frame_recv_callback);
	nghttp2_session_callbacks_set_on_stream_close_callback(
			callbacks, on_stream_close_callback);
	nghttp2_session_callbacks_set_on_header_callback(callbacks,
			on_header_callback);
	nghttp2_session_callbacks_set_on_begin_headers_callback(
			callbacks, on_begin_headers_callback);
	nghttp2_session_callbacks_set_on_data_chunk_recv_callback( callbacks,
			on_data_chunk_recv_callback );

	nghttp2_session_callbacks_set_on_frame_send_callback( callbacks, frame_send_callback );
	nghttp2_session_callbacks_set_on_frame_not_send_callback( callbacks, frame_not_send_callback );

	nghttp2_session_callbacks_set_before_frame_send_callback( callbacks, before_frame_send_callback );

	nghttp2_session* ngsession;
	nghttp2_session_client_new3( &ngsession, callbacks, this, options, &all );

	nghttp2_session_callbacks_del(callbacks);

	session_data.reset( ngsession );
}

bool session_client::start() noexcept
{
	LOGTRACE("start");
	if (connector() == nullptr)
			return false;
	send_connection_header();

	do_write();
	return true;
}

void session_client::send_connection_header()
{
	LOGTRACE("send_connection_header");
	constexpr const int ivlen = 2;
	// all these settings pairs are copied by nghttp2_submit_settings, so a temporary will be fine
	nghttp2_settings_entry iv[ivlen] = {
		{NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS, max_concurrent_streams},
		{NGHTTP2_SETTINGS_ENABLE_PUSH, 0}
	};

	int r = nghttp2_submit_settings( session_data.get(), NGHTTP2_FLAG_NONE, iv, ivlen );

	// If this happens is a bug - die with fireworks
	if ( r ) THROW( errors::setting_connection_failure, r );

	do_write();
}

http2::stream_client* session_client::create_stream( std::int32_t id )
{
	stream_client* stream_data = static_cast<stream_client*>( all.malloc( sizeof(stream_client), all.mem_user_data ) );
	LOGTRACE(" Create stream: ", id, " address: ", stream_data );
	new ( stream_data ) stream_client{ this->get_shared(), [stream_data] ( stream_client* s, session_client* sess )
			{
					LOGTRACE("Stream destruction: ", s, " session: ", sess );
					s->~stream_client();
					sess->all.free( s, sess->all.mem_user_data );
			}
	}; // The pointed object will commit suicide

	stream_data->id( id );
	//todo: replace.
	auto res_handler = std::make_shared<http::client_response>(this->get_shared(), connector()->io_service());
	auto req_handler = std::make_shared<http::client_request>([stream_data](http::http_request&& req)
	{
		stream_data->on_header(std::move(req));
	},
	[stream_data](auto&& b, auto len){
		stream_data->on_body(std::move(b), len);
	},
	[stream_data](auto&& k, auto&& v) {
		stream_data->on_trailer(std::move(k), std::move(v));
	},
	[stream_data]() {
		stream_data->on_eom();
	}, connector()->io_service());

	stream_data->set_handlers(req_handler, res_handler);

	int rv = nghttp2_session_set_stream_user_data( session_data.get(), id, stream_data );
	if ( rv != 0 ) LOGERROR ( "nghttp2_session_set_stream_user_data ",  nghttp2_strerror( rv ) );

	++stream_counter;
	LOGTRACE(" stream is ", stream_data, " stream counter ", stream_counter );
	return stream_data;
}

void session_client::go_away()
{
	std::int32_t last_id = nghttp2_session_get_last_proc_stream_id( session_data.get() );
	std::uint32_t error_code = NGHTTP2_INTERNAL_ERROR;
	int r = nghttp2_submit_goaway( session_data.get(), NGHTTP2_FLAG_NONE, last_id, error_code, nullptr, 0 );
	if ( r < 0 )
	{
			// No mem or invalid data - they look like a bug to me
//		LOGERROR( "nghttp2_submit_goaway ", nghttp2_strerror( r ) );
			//todo: avoid throwing
			//nice ;)
			THROW( errors::failing_at_failing, r );
	}

	gone = true;
}

int session_client::on_begin_headers_callback( nghttp2_session *session_, const nghttp2_frame *frame, void *user_data )
{
	LOGTRACE("on_begin_headers_callback - id: ", frame->hd.stream_id );
	stream_client* stream_data = get_stream_user_data( session_, frame->hd.stream_id );
	if(!stream_data) // this means that we have not created this stream by ourselves
	{
		session_client* s_this = static_cast<session_client*>( user_data );
		s_this->create_stream( frame->hd.stream_id );
	}
	return 0;
}

int session_client::on_header_callback( nghttp2_session *session_,
		const nghttp2_frame *frame, const uint8_t *name, size_t namelen, const uint8_t *value,
		size_t valuelen, uint8_t flags, void *user_data )
{
	LOGTRACE("on_header_callback");
	session_client* s_this = static_cast<session_client*>( user_data );
	if ( s_this == nullptr )
	{
		LOGERROR( "s_this nullptr on_header_callback" );
		return 0;
	}

	static const char AUTHORITY[] = ":authority";
	static const char STATUS[] = ":status";
	switch (frame->hd.type) //fixme
	{
	case NGHTTP2_HEADERS:
		if (frame->headers.cat == NGHTTP2_HCAT_REQUEST) break;

		stream_client* stream_data = get_stream_user_data( session_, frame->hd.stream_id );
		if ( ! stream_data ) break;

		if ( namelen == sizeof(AUTHORITY) - 1 && memcmp(AUTHORITY, name, namelen ) == 0 )
		{
			std::string uri_host{value, value + valuelen};
			stream_data->uri_host( uri_host );
		}
		else if ( namelen == sizeof(STATUS) - 1 && memcmp(STATUS, name, namelen ) == 0 )
		{
			std::string status{value, value + valuelen};
			stream_data->status( std::stoi(status) );
		}
		else // Normal headers
		{
			std::string key{name, name + namelen};
			std::string val{value, value + valuelen};
			stream_data->add_header( key, val );
		}
		break;
	}

	if ( nghttp2_session_want_write( s_this->session_data.get() ) )
		s_this->do_write();
	return 0;
}

void session_client::trigger_timeout_event()
{
	if(auto s = connector())
	{
			return s->io_service().post([sh=shared_from_this(), this](){ http::client_connection::timeout(); });
	}
	assert(false);
}

std::shared_ptr<session_client> session_client::get_shared()
{
	return std::static_pointer_cast<session_client>(this->shared_from_this());
}

std::pair<std::shared_ptr<http::client_response>, std::shared_ptr<http::client_request> > session_client::get_user_handlers()
{
	auto stream = this->create_stream( -1 );
	return {stream->res, stream->req};
}

void session_client::do_write()
{
	LOGTRACE("do_write");
	if(connector())
			connector()->do_write();
	else
	{
			LOGERROR("session http2 ", this, " connector already destroyed");
			// go on with handler work, so that nghttp2 can call the generate_cb
	}
}

void session_client::close()
{
	user_close = true;
	if(auto s = connector()) s->close();
}

session_client::~session_client()
{
	nghttp2_option_del( options );
}

void session_client::set_timeout(std::chrono::milliseconds ms) {
	if(auto s = connector())
	{
			s->set_timeout(ms);
	}
}

void session_client::on_connector_nulled()
{
	//todo: send events to all streams involved!
	LOGTRACE("on_connector_nulled");
	//send back error to connector.
	auto err = (user_close) ? http::error_code::connection_closed : http::error_code::closed_by_client;
	http::client_connection::error(err);
	for(auto &cbs : pending)
	{
			cbs.second();
	}
	notify_error(err);

// 	s->on_request_canceled(error_code_distruction)
	// Streams should be destroyed by nghttp2

// 	if( should_stop() )
// 		delete this;
// 	else
// 		for(auto &s: streams) s->on_request_canceled(error_code_distruction);
}

void session_client::notify_error(http::error_code ec)
{
	std::for_each(listeners.begin(), listeners.end(), [&ec](auto *s){
			s->notify_error(ec);
	});
}

void session_client::finished_stream() noexcept
{
	--stream_counter;
}

int session_client::on_frame_recv_callback(nghttp2_session *session_, const nghttp2_frame *frame, void *user_data )
{
	session_client* s_this = static_cast<session_client*>( user_data );
	LOGTRACE( "on_frame_recv_callback Stream id: ", frame->hd.stream_id , " type ", (int)frame->hd.type );

	int32_t stream_id = frame->hd.stream_id;
	stream_client* stream_data = get_stream_user_data( s_this->session_data.get(), stream_id );
	if ( stream_data != nullptr  )
	{
		if ( frame->hd.type == NGHTTP2_HEADERS )
		{
			if ( frame->headers.cat != NGHTTP2_HCAT_REQUEST )
				stream_data->on_response_header_complete();
			else
				LOGERROR("Strangeness in HTTP2 Headers");
		}

		// bitwise operator is not an error
		if ( frame->hd.flags & NGHTTP2_FLAG_END_STREAM )
			stream_data->on_response_finished(); // 1 stream, 1 message/response ← probably false
	}

	if ( nghttp2_session_want_write( s_this->session_data.get() ) )
		s_this->do_write();
	return 0;
}

int session_client::on_stream_close_callback( nghttp2_session* session_, int32_t stream_id, uint32_t error_code, void *user_data )
{
	LOGTRACE( "Stream close callback ", stream_id );
	session_client* s_this = static_cast<session_client*>( user_data );
	stream_client* stream_data = get_stream_user_data( s_this->session_data.get(), stream_id );
	assert( stream_data );
	stream_data->die();

	if ( nghttp2_session_want_write( s_this->session_data.get() ) )
			s_this->do_write();
	return 0;
}

bool session_client::should_stop() const noexcept
{
	return nghttp2_session_want_read( session_data.get() ) == 0 &&
			nghttp2_session_want_write( session_data.get() ) == 0;
}

int session_client::on_data_chunk_recv_callback(nghttp2_session *session_, uint8_t flags, int32_t stream_id,
		const uint8_t * data, size_t len, void *user_data )
{
	session_client* s_this = static_cast<session_client*>( user_data );
	stream_client* res = get_stream_user_data( session_, stream_id );

	if(res)
	{
		auto ptr = std::make_unique<char[]>(len);
		std::copy(data, data + len, ptr.get()); /// @warning : we don't have ownership.
		res->on_response_body( std::move(ptr), len );
		/// @note return  NGHTTP2_ERR_PAUSE ; to pause input
	}

	if ( nghttp2_session_want_write( s_this->session_data.get() ) )
			s_this->do_write();

	return 0;
}

int session_client::frame_send_callback (nghttp2_session *session_, const nghttp2_frame *frame, void *user_data )
{
// 	session_client* s_this = static_cast<session_client*>( user_data );
	std::int32_t stream_id = frame->hd.stream_id;
	LOGTRACE("frame_send_callback - Stream id: ", stream_id, ", type ", (int)frame->hd.type );
//todo: for http2 push
		//if (frame->hd.type != NGHTTP2_PUSH_PROMISE) ← only usage in his library
// 	void* stud = nghttp2_session_get_strea_muser_data( session_, stream_id );
//
// 	if ( stud != nullptr )
// 	{
// 		stream_client* stream_p = static_cast<stream_client*>( stud );
// // 		stream_p->flush();
//
// 	}
		return 0;
}

int session_client::frame_not_send_callback ( nghttp2_session *session_, const nghttp2_frame *frame,
		int lib_error_code, void *user_data )
{
	std::int32_t stream_id = frame->hd.stream_id;
	LOGERROR( "Frame not sent: ", nghttp2_strerror( lib_error_code ), " stream id: ", stream_id );

	// These get ignored in library's implementation
	if ( frame->hd.type != NGHTTP2_HEADERS || frame->headers.cat != NGHTTP2_HCAT_REQUEST )
			return 0;

	void* stud = nghttp2_session_get_stream_user_data( session_, stream_id );
	if ( stud != nullptr )
	{
			stream_client* stream_p = static_cast<stream_client*>( stud );
			stream_p->die();
	}
	// Issue RST_STREAM so that stream does not hang around.
	int rc = nghttp2_submit_rst_stream(session_, NGHTTP2_FLAG_NONE, stream_id, NGHTTP2_INTERNAL_ERROR);
	if ( rc != 0 ) // No mem or bug
			THROW( errors::failing_at_failing, rc );
//		LOGERROR( "RST STREAM ", stream_id, " ", nghttp2_strerror( rc ) );

	return 0;
}

int session_client::before_frame_send_callback(nghttp2_session* session, const nghttp2_frame* frame, void* user_data)
{
	// TODO: since now on we can send frames
	return {};
}

stream_client*session_client::get_stream_user_data(nghttp2_session* session, int32_t stream_id)
{
	auto data = nghttp2_session_get_stream_user_data(session, stream_id);
	return static_cast<stream_client*>(data);
}

bool session_client::on_write(std::string& ch )
{
	LOGTRACE("on_write");

	const uint8_t* data;
	int consumed = nghttp2_session_mem_send( session_data.get(), &data );

	if ( consumed < 0 ) // Memory exhausted!
			THROW ( errors::session_send_failure, consumed );

	if ( consumed >= 0 )
	{
			std::string tdata{data, data + static_cast<size_t>(consumed)};
			ch = std::move( tdata );
	}
	std::cout << "client write: " << ch << std::endl;
	if ( connector() == nullptr ) return false;
	return true;
}

bool session_client::on_read(const char* data, size_t len)
{
	LOGTRACE("on_read");
	std::cout << "client on_read: " << std::string{data, data + len} << std::endl;
	int rv = nghttp2_session_mem_recv( session_data.get(), reinterpret_cast<const uint8_t*>(data), len );

	if ( rv < 0 )
	{
			if ( rv == NGHTTP2_ERR_FLOODED || rv == NGHTTP2_ERR_CALLBACK_FAILURE )
					go_away();
			else // No mem or bad client magic ( unrecoverable error and upstream error )
					return false;
	}
	assert( static_cast<unsigned int>(rv)== len );

	if ( nghttp2_session_want_write( session_data.get() ) )
			do_write();
	return true;
}

}



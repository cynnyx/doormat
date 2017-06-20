#include <nghttp2/nghttp2.h>
#include <gtest/gtest.h>
#include "../../../src/http/server/request.h"
#include "../../../src/http/server/response.h"
#include "../../mocks/mock_connector/mock_connector.h"
#include "../src/http2/session.h"
#include "../src/http2/stream.h"
#include "../../../src/utils/log_wrapper.h"

#include <memory>
#include <utility>
#include <limits>

// Sorry man!
#define MAKE_NV(NAME, VALUE)                                                   \
  {                                                                            \
    (uint8_t *) NAME, (uint8_t *)VALUE, sizeof(NAME) - 1, sizeof(VALUE) - 1,   \
        NGHTTP2_NV_FLAG_NONE                                                   \
  }

#define MAKE_NV_CS(NAME, VALUE)                                                \
  {                                                                            \
    (uint8_t *) NAME, (uint8_t *)VALUE, sizeof(NAME) - 1, strlen(VALUE),       \
        NGHTTP2_NV_FLAG_NONE                                                   \
  }

static void diec(const char *func, int error_code) 
{
	fprintf(stderr, "FATAL: %s: error_code=%d, msg=%s\n", func, error_code,
		nghttp2_strerror(error_code));
}

using server_connection_t = http2::session;

class http2_server_test : public ::testing::Test
{
	static http2_server_test* tx;
public:
	http2_server_test() { tx = this; }
	void SetUp()
	{
		data_recv = 0;
		header_recv = 0;
		stream_terminated = false;
		session_terminated = false;
		closing_error_code = -1;
		data_recv_v.clear();
		_write_cb = [this](std::string d)
		{
			std::string chunk{ d.data(), d.size() };
			response_raw += chunk;
			LOGTRACE("Append response ", response_raw.size(), " appended ", d.size(), " bytes ");
		};
		mock_connector = std::make_shared<MockConnector>(io, _write_cb);
		_handler = std::make_shared<server_connection_t>();
		mock_connector->handler(_handler);
		go_away = false;
	}

	boost::asio::io_service io;
	std::shared_ptr<MockConnector> mock_connector;
	std::shared_ptr<server_connection_t> _handler;
	MockConnector::wcb _write_cb;
 	std::string response_raw;
 	std::string request_raw;
	bool go_away{false};
	~http2_server_test() { tx = nullptr; }
	
	static ssize_t read( char* buf, std::size_t len )
	{
		LOGTRACE( "Read ", len, " running! ", tx->response_raw.size(), " bytes ");
		std::size_t r = tx->response_raw.size();
		r =  len < r ? len : r;
		memcpy(buf, tx->response_raw.data(), r );
		if ( r > 0 )
			tx->response_raw.erase(0, r);
		return static_cast<ssize_t>(r);
	}
	
	static ssize_t write( const char* buf, std::size_t len )
	{
		LOGTRACE("Write ", len, " bytes ");
		std::string local_buf{buf, len};
		tx->request_raw += local_buf; // Append truncates nulls!
		return len;
	}
	
	static int data_recv;
	static int header_recv;
	static int stream_terminated;
	static bool session_terminated;
	static int closing_error_code;
	static std::vector<std::string> data_recv_v;
	static std::vector<std::multimap<std::string, std::string>> header_recv_v;
};

http2_server_test* http2_server_test::tx{nullptr};
int http2_server_test::data_recv{0};
int http2_server_test::header_recv{0};
int http2_server_test::stream_terminated{0};
bool http2_server_test::session_terminated{false};
int http2_server_test::closing_error_code{-1};
std::vector<std::string> http2_server_test::data_recv_v;
std::vector<std::multimap<std::string, std::string>> http2_server_test::header_recv_v;

struct Connection 
{
	nghttp2_session *session;
	http2_server_test* test;
};

struct Request 
{
	const char *host;
	/* In this program, path contains query component as well. */
	const char *path;
	/* This is the concatenation of host and port with ":" in
		between. */
	const char *hostport;
	/* Stream ID for this request. */
	int32_t stream_id;
	uint16_t port;
};

static int test_read(char* buf, std::size_t len )
{
	LOGTRACE("Reading");
	return http2_server_test::read( buf, len );
}

static int test_write( const char* buf, std::size_t len )
{
	LOGTRACE("Writing");
	return http2_server_test::write( buf, len );
}

static ssize_t send_callback( nghttp2_session *session, 
	const uint8_t *data, size_t length, int flags, void * user_data )
{
	return test_write( (const char*)data, length );
}

static ssize_t recv_callback(nghttp2_session *session , uint8_t *buf,
	size_t length, int flags, void *user_data) 
{
	int rv = test_read( (char*)buf, length ); 
	return rv;
}

static int on_stream_close_callback(nghttp2_session *session, int32_t stream_id,
	uint32_t error_code, void *user_data) 
{
	LOGTRACE("stream close cb");
	Request *req;
	req = static_cast<Request*> ( nghttp2_session_get_stream_user_data(session, stream_id) );
	if (req) 
	{
		int rv = nghttp2_session_terminate_session(session, NGHTTP2_NO_ERROR);

		if (rv != 0)
			diec("nghttp2_session_terminate_session", rv);
	}
	++http2_server_test::stream_terminated;
	http2_server_test::closing_error_code = error_code;
	return 0;
}

static int on_frame_send_callback(nghttp2_session *session,
	const nghttp2_frame *frame, void *user_data) 
{
	LOGTRACE("FRAME SEND");
// 	size_t i;
	switch (frame->hd.type)
	{
	case NGHTTP2_HEADERS:
		if (nghttp2_session_get_stream_user_data(session, frame->hd.stream_id)) 
		{
// 			const nghttp2_nv *nva = frame->headers.nva;
			LOGTRACE("[INFO] C ----------------------------> S (HEADERS)");
// 			for (i = 0; i < frame->headers.nvlen; ++i) 
// 			{
// 				fwrite(nva[i].name, nva[i].namelen, 1, stdout);
// 				printf(": ");
// 				fwrite(nva[i].value, nva[i].valuelen, 1, stdout);
// 				printf("\n");
// 			}
		}
		break;
	case NGHTTP2_RST_STREAM:
		LOGTRACE("[INFO] C ----------------------------> S (RST_STREAM)\n");
		break;
	case NGHTTP2_GOAWAY:
		LOGTRACE("[INFO] C ----------------------------> S (GOAWAY)\n");
		break;
	}
	return 0;
}

static int on_frame_recv_callback(nghttp2_session *session,
	const nghttp2_frame *frame, void *user_data ) 
{
	LOGTRACE( "C recv frame" );
	size_t i;
	switch (frame->hd.type) 
	{
	case NGHTTP2_HEADERS:
	if (frame->headers.cat == NGHTTP2_HCAT_RESPONSE) 
	{
			const nghttp2_nv *nva = frame->headers.nva;
			Request *req;
			req = static_cast<Request*>( nghttp2_session_get_stream_user_data(session, frame->hd.stream_id) );
			if (req) 
			{
				printf("[INFO] C <---------------------------- S (HEADERS)\n");
				for (i = 0; i < frame->headers.nvlen; ++i) 
				{
					fwrite(nva[i].name, nva[i].namelen, 1, stdout);
					printf(": ");
					fwrite(nva[i].value, nva[i].valuelen, 1, stdout);
					printf("\n");
				}
			}
		}
		++http2_server_test::header_recv;
		break;
		case NGHTTP2_RST_STREAM:
		LOGTRACE("[INFO] C <---------------------------- S (RST_STREAM)");
		break;
		case NGHTTP2_GOAWAY:
		LOGTRACE("[INFO] C <---------------------------- S (GOAWAY)");
		break;
	}
	return 0;
}

/*
 * The implementation of nghttp2_on_data_chunk_recv_callback type. We
 * use this function to print the received response body.
 */
static int on_data_chunk_recv_callback(nghttp2_session *session,
	uint8_t flags, int32_t stream_id, const uint8_t *data, size_t len,
	void *user_data) 
{
	LOGTRACE("CHUNK RECV");
	Request *req = static_cast<Request *>( nghttp2_session_get_stream_user_data(session, stream_id) );
	if (req)
	{
		LOGTRACE("[INFO] C <---------------------------- S (DATA chunk)\n", (unsigned long int)len, " bytes");
		LOGTRACE( std::string{ (const char*)data, len } );
		
		++http2_server_test::data_recv;
	}
	http2_server_test::data_recv_v.resize(stream_id + 1);
	http2_server_test::data_recv_v[stream_id].append(data, data + len);
	return 0;
}

static int on_header_callback(nghttp2_session *session, const nghttp2_frame *frame,
	const uint8_t *name, size_t namelen, const uint8_t *value,
	size_t valuelen, uint8_t flags, void *user_data)
{
	LOGTRACE("HEADER RECV: ", std::string{name, name + namelen}, ": ", std::string{value, value + valuelen});

	http2_server_test::header_recv_v.resize(frame->hd.stream_id + 1);
	http2_server_test::header_recv_v[frame->hd.stream_id].emplace(std::string{name, name + namelen}, 
		std::string{value, value + valuelen});
	return 0;
}

static void setup_nghttp2_callbacks(nghttp2_session_callbacks *callbacks) 
{
	nghttp2_session_callbacks_set_send_callback(callbacks, send_callback);
	nghttp2_session_callbacks_set_recv_callback(callbacks, recv_callback);
	nghttp2_session_callbacks_set_on_frame_send_callback(callbacks,
		on_frame_send_callback);
	nghttp2_session_callbacks_set_on_frame_recv_callback(callbacks,
		on_frame_recv_callback);
	nghttp2_session_callbacks_set_on_stream_close_callback(
		callbacks, on_stream_close_callback);
	nghttp2_session_callbacks_set_on_data_chunk_recv_callback(
		callbacks, on_data_chunk_recv_callback);
	nghttp2_session_callbacks_set_on_header_callback(callbacks, on_header_callback);
}

static void exec_io(struct Connection *connection) 
{
	int rv = nghttp2_session_recv(connection->session);
	if (rv != 0)
		diec("nghttp2_session_recv", rv);

	rv = nghttp2_session_send(connection->session);
	if (rv != 0)
		diec("nghttp2_session_send", rv);
}

static std::unique_ptr<Connection> start_client()
{
	nghttp2_session_callbacks *callbacks;
	
	std::unique_ptr<Connection> c{ new Connection };
	Connection* connection = c.get();
	auto rv = nghttp2_session_callbacks_new(&callbacks);
	if ( rv != 0 ) throw rv;
	setup_nghttp2_callbacks(callbacks);
	
	rv = nghttp2_session_client_new(&(connection->session), callbacks, &connection);
	nghttp2_session_callbacks_del(callbacks);
	if ( rv != 0 ) throw rv; // If we have to throw it's better when callbacks are freed
	return c;
}

/*
 * Submits the request |req| to the connection |connection|.  This
 * function does not send packets; just append the request to the
 * internal queue in |connection->session|.
 */
static void submit_request(struct Connection *connection, struct Request *req) 
{
	int32_t stream_id;
	/* Make sure that the last item is NULL */
	const nghttp2_nv nva[] = {
		MAKE_NV(":method", "GET"), MAKE_NV_CS(":path", req->path),
		MAKE_NV(":scheme", "https"), MAKE_NV_CS(":authority", req->hostport),
		MAKE_NV("accept", "*/*"),
		MAKE_NV("user-agent", "nghttp2/" NGHTTP2_VERSION)};

	stream_id = nghttp2_submit_request(connection->session, NULL, nva,
		sizeof(nva) / sizeof(nva[0]), NULL, req);

	if (stream_id < 0)
		diec("nghttp2_submit_request", stream_id);

	req->stream_id = stream_id;
	LOGTRACE("[INFO] Stream ID = ", stream_id);
}

static void submit_settings(struct Connection* connection)
{
	nghttp2_submit_settings(connection->session, NGHTTP2_FLAG_NONE, NULL, 0);
}


// TODO this is a duplicate - bring it out
static std::unique_ptr<char[]> make_data_ptr(const std::string& s)
{
	auto ptr = std::make_unique<char[]>(s.size());
	std::copy(s.begin(), s.end(), ptr.get());
	return std::move(ptr);
}

TEST_F(http2_server_test, connection)
{
	static const std::string body{"Ave client, dummy node says hello"};
	static const std::string content_type{"text/plain"};
	static const std::string host{"cristo.it"};
	std::unique_ptr<Connection>  c = start_client();
	Connection* cnx =  c.get();
	cnx->test = this;

	_handler->on_request([&](auto conn, auto req, auto res)
	{
		req->on_finished([res](auto req)
		{
			http::http_response r;
			r.protocol(http::proto_version::HTTP20);
			r.status(200);
			r.header("content-type", content_type);
			r.header("date", "Tue, 17 May 2016 14:53:09 GMT");
			r.hostname(host);
			r.content_len(body.size());
			res->headers(std::move(r));
			res->body(make_data_ptr(body), body.size());
			res->end();
		});
	});
	
	auto keep_alive = std::make_unique<boost::asio::io_service::work>(mock_connector->io_service());
	bool terminated{false};
	std::function<void()> io_poll;
	io_poll = [&io_poll, &keep_alive, cnx, &terminated, this] 
	{
		if (nghttp2_session_want_read(cnx->session) ||
			nghttp2_session_want_write(cnx->session))
		{
			exec_io( cnx );
			mock_connector->read( request_raw );
			request_raw = "";
			mock_connector->io_service().post(io_poll);
		}
		else
		{
			terminated = true;
			keep_alive.reset();
		}
	};
	mock_connector->io_service().post([this, &terminated, cnx, &io_poll]()
	{
		Request req;
		Request* greq = &req;
		greq->path = "/";
		greq->stream_id = -1;
		greq->hostport = "80";

		submit_settings(cnx);

		submit_request(cnx, greq);

		io_poll();
	});
	mock_connector->io_service().run();
	nghttp2_session_del( cnx->session );

	ASSERT_TRUE( terminated );
	EXPECT_EQ( http2_server_test::stream_terminated, 1 );
	EXPECT_EQ( http2_server_test::data_recv, 1 );
	EXPECT_EQ( http2_server_test::header_recv, 1 );
	EXPECT_EQ( http2_server_test::closing_error_code, NGHTTP2_NO_ERROR );
	EXPECT_EQ( http2_server_test::data_recv_v[1], body );
	EXPECT_EQ( http2_server_test::header_recv_v[1].find(":status")->second, std::string{"200"} );
	EXPECT_EQ( http2_server_test::header_recv_v[1].find("content-length")->second, std::to_string(body.size()) );
	EXPECT_EQ( http2_server_test::header_recv_v[1].find("content-type")->second, content_type );
	EXPECT_EQ( http2_server_test::header_recv_v[1].find("host")->second, host );
}

TEST_F(http2_server_test, multiple_request)
{
	static const std::string body{"Ave client, dummy node says hello"};
	static const std::string content_type{"text/plain"};
	static const std::string host{"cristo.it"};
	
	static const std::string body2{"Ave client, dummy node says hello hello"};
	std::unique_ptr<Connection>  c = start_client();
	Connection* cnx =  c.get();
	cnx->test = this;

	int request_done = 0;
	_handler->on_request([&](auto conn, auto req, auto res)
	{
		req->on_finished([res, &request_done](auto req)
		{
			http::http_response r;
			r.protocol(http::proto_version::HTTP20);
			r.status(200);
			r.header("content-type", content_type);
			r.header("date", "Tue, 17 May 2016 14:53:09 GMT");
			r.hostname(host);
			if ( request_done == 0 )
			{
				r.content_len(body.size());
				res->headers(std::move(r));
				res->body(make_data_ptr(body), body.size());
				++request_done;
			}
			else
			{
				r.content_len(body2.size());
				res->headers(std::move(r));
				res->body(make_data_ptr(body2), body2.size());
			}
			res->end();
		});
	});
	
	auto keep_alive = std::make_unique<boost::asio::io_service::work>(mock_connector->io_service());
	bool terminated{false};
	std::function<void()> io_poll;
	io_poll = [&io_poll, &keep_alive, cnx, &terminated, this] 
	{
		if (nghttp2_session_want_read(cnx->session) ||
			nghttp2_session_want_write(cnx->session))
		{
			exec_io( cnx );
			mock_connector->read( request_raw );
			request_raw = "";
			mock_connector->io_service().post(io_poll);
		}
		else
		{
			terminated = true;
			keep_alive.reset();
		}
	};
	mock_connector->io_service().post([this, &terminated, cnx, &io_poll]()
	{
		Request req;
		Request* greq = &req;
		greq->path = "/";
		greq->stream_id = -1;
		greq->hostport = "80";

		submit_settings(cnx);

		submit_request(cnx, greq);
		
		Request req2;
		Request* greq2 = &req2;
		greq2->path = "/second";
		greq2->stream_id = -1;
		greq2->hostport = "80";
		
		submit_request(cnx, greq2);

		io_poll();
	});
	mock_connector->io_service().run();
	nghttp2_session_del( cnx->session );

	ASSERT_TRUE( terminated );
	EXPECT_EQ( http2_server_test::stream_terminated, 2 );
	EXPECT_EQ( http2_server_test::data_recv, 2 );
	EXPECT_EQ( http2_server_test::header_recv, 2 );
	EXPECT_EQ( http2_server_test::closing_error_code, NGHTTP2_NO_ERROR );
	EXPECT_EQ( http2_server_test::data_recv_v[1], body );
	EXPECT_EQ( http2_server_test::header_recv_v[1].find(":status")->second, std::string{"200"} );
	EXPECT_EQ( http2_server_test::header_recv_v[1].find("content-length")->second, std::to_string(body.size()) );
	EXPECT_EQ( http2_server_test::header_recv_v[1].find("content-type")->second, content_type );
	EXPECT_EQ( http2_server_test::header_recv_v[1].find("host")->second, host );
	
	EXPECT_EQ( http2_server_test::data_recv_v[3], body2 );
	EXPECT_EQ( http2_server_test::header_recv_v[3].find(":status")->second, std::string{"200"} );
	EXPECT_EQ( http2_server_test::header_recv_v[3].find("content-length")->second, std::to_string(body2.size()) );
	EXPECT_EQ( http2_server_test::header_recv_v[3].find("content-type")->second, content_type );
	EXPECT_EQ( http2_server_test::header_recv_v[3].find("host")->second, host );
}

TEST_F(http2_server_test, trailer_recv)
{
	static const std::string body{"Ave client, dummy node says hello"};
	static const std::string content_type{"text/plain"};
	static const std::string host{"cristo.it"};
	std::unique_ptr<Connection>  c = start_client();
	Connection* cnx =  c.get();
	cnx->test = this;

	_handler->on_request([&](auto conn, auto req, auto res)
	{
		req->on_finished([res](auto req)
		{
			http::http_response r;
			r.protocol(http::proto_version::HTTP20);
			r.status(200);
			r.header("content-type", content_type);
			r.header("date", "Tue, 17 May 2016 14:53:09 GMT");
			r.hostname(host);
			r.content_len(body.size());
			res->headers(std::move(r));
			res->body(make_data_ptr(body), body.size());
			res->trailer("francesco", "in ritardo");
			res->trailer("francesco", "a zoppo galletto");
			res->end();
		});
	});
	
	auto keep_alive = std::make_unique<boost::asio::io_service::work>(mock_connector->io_service());
	bool terminated{false};
	std::function<void()> io_poll;
	io_poll = [&io_poll, &keep_alive, cnx, &terminated, this] 
	{
		if (nghttp2_session_want_read(cnx->session) ||
			nghttp2_session_want_write(cnx->session))
		{
			exec_io( cnx );
			mock_connector->read( request_raw );
			request_raw = "";
			mock_connector->io_service().post(io_poll);
		}
		else
		{
			terminated = true;
			keep_alive.reset();
		}
	};
	mock_connector->io_service().post([this, &terminated, cnx, &io_poll]()
	{
		Request req;
		Request* greq = &req;
		greq->path = "/";
		greq->stream_id = -1;
		greq->hostport = "80";

		submit_settings(cnx);

		submit_request(cnx, greq);

		io_poll();
	});
	mock_connector->io_service().run();
	nghttp2_session_del( cnx->session );

	ASSERT_TRUE( terminated );
	EXPECT_EQ( http2_server_test::stream_terminated, 1 );
	EXPECT_EQ( http2_server_test::data_recv, 1 );
	// Trailers are mixed up with headers
	EXPECT_EQ( http2_server_test::header_recv, 2 );
	EXPECT_EQ( http2_server_test::closing_error_code, NGHTTP2_NO_ERROR );
	EXPECT_EQ( http2_server_test::data_recv_v[1], body );
	EXPECT_EQ( http2_server_test::header_recv_v[1].find(":status")->second, std::string{"200"} );
	EXPECT_EQ( http2_server_test::header_recv_v[1].find("content-length")->second, std::to_string(body.size()) );
	EXPECT_EQ( http2_server_test::header_recv_v[1].find("content-type")->second, content_type );
	EXPECT_EQ( http2_server_test::header_recv_v[1].find("host")->second, host );
}

TEST_F(http2_server_test, missing_settings)
{
	static const std::string body{"Ave client, dummy node says hello"};
	static const std::string content_type{"text/plain"};
	static const std::string host{"cristo.it"};
	std::unique_ptr<Connection>  c = start_client();
	Connection* cnx =  c.get();
	cnx->test = this;

	_handler->on_request([&](auto conn, auto req, auto res)
	{
		req->on_finished([res](auto req)
		{
			http::http_response r;
			r.protocol(http::proto_version::HTTP20);
			r.status(200);
			r.header("content-type", content_type);
			r.header("date", "Tue, 17 May 2016 14:53:09 GMT");
			r.hostname(host);
			r.content_len(body.size());
			res->headers(std::move(r));
			res->body(make_data_ptr(body), body.size());
			res->end();
		});
	});
	
	auto keep_alive = std::make_unique<boost::asio::io_service::work>(mock_connector->io_service());
	bool terminated{false};
	std::function<void()> io_poll;
	io_poll = [&io_poll, &keep_alive, cnx, &terminated, this] 
	{
		if (nghttp2_session_want_read(cnx->session) ||
			nghttp2_session_want_write(cnx->session))
		{
			exec_io( cnx );
			mock_connector->read( request_raw );
			request_raw = "";
			mock_connector->io_service().post(io_poll);
		}
		else
		{
			terminated = true;
			keep_alive.reset();
		}
	};
	mock_connector->io_service().post([this, &terminated, cnx, &io_poll]()
	{
		Request req;
		Request* greq = &req;
		greq->path = "/";
		greq->stream_id = -1;
		greq->hostport = "80";

// 		submit_settings(cnx);

		submit_request(cnx, greq);

		io_poll();
	});
	mock_connector->io_service().run();
	nghttp2_session_del( cnx->session );

	ASSERT_TRUE( terminated );
	EXPECT_EQ( http2_server_test::stream_terminated, 1 );
	EXPECT_EQ( http2_server_test::data_recv, 0 );
	EXPECT_EQ( http2_server_test::header_recv, 0 );
	EXPECT_EQ( http2_server_test::closing_error_code,  NGHTTP2_CANCEL );
}

TEST_F(http2_server_test, double_settings)
{
	static const std::string body{"Ave client, dummy node says hello"};
	static const std::string content_type{"text/plain"};
	static const std::string host{"cristo.it"};
	
	static const std::string body2{"Ave client, dummy node says hello hello"};
	std::unique_ptr<Connection>  c = start_client();
	Connection* cnx =  c.get();
	cnx->test = this;

	int request_done = 0;
	_handler->on_request([&](auto conn, auto req, auto res)
	{
		req->on_finished([res, &request_done](auto req)
		{
			http::http_response r;
			r.protocol(http::proto_version::HTTP20);
			r.status(200);
			r.header("content-type", content_type);
			r.header("date", "Tue, 17 May 2016 14:53:09 GMT");
			r.hostname(host);
			if ( request_done == 0 )
			{
				r.content_len(body.size());
				res->headers(std::move(r));
				res->body(make_data_ptr(body), body.size());
				++request_done;
			}
			else
			{
				r.content_len(body2.size());
				res->headers(std::move(r));
				res->body(make_data_ptr(body2), body2.size());
			}
			res->end();
		});
	});
	
	auto keep_alive = std::make_unique<boost::asio::io_service::work>(mock_connector->io_service());
	bool terminated{false};
	std::function<void()> io_poll;
	io_poll = [&io_poll, &keep_alive, cnx, &terminated, this] 
	{
		if (nghttp2_session_want_read(cnx->session) ||
			nghttp2_session_want_write(cnx->session))
		{
			exec_io( cnx );
			mock_connector->read( request_raw );
			request_raw = "";
			mock_connector->io_service().post(io_poll);
		}
		else
		{
			terminated = true;
			keep_alive.reset();
		}
	};
	mock_connector->io_service().post([this, &terminated, cnx, &io_poll]()
	{
		Request req;
		Request* greq = &req;
		greq->path = "/";
		greq->stream_id = -1;
		greq->hostport = "80";

		submit_settings(cnx);

		submit_request(cnx, greq);
		
		Request req2;
		Request* greq2 = &req2;
		greq2->path = "/second";
		greq2->stream_id = -1;
		greq2->hostport = "80";
		
		submit_settings(cnx);
		submit_request(cnx, greq2);

		io_poll();
	});
	mock_connector->io_service().run();
	nghttp2_session_del( cnx->session );

	ASSERT_TRUE( terminated );
	EXPECT_EQ( http2_server_test::stream_terminated, 2 );
	EXPECT_EQ( http2_server_test::data_recv, 2 );
	EXPECT_EQ( http2_server_test::header_recv, 2 );
	EXPECT_EQ( http2_server_test::closing_error_code, NGHTTP2_NO_ERROR );
	EXPECT_EQ( http2_server_test::data_recv_v[1], body );
	EXPECT_EQ( http2_server_test::header_recv_v[1].find(":status")->second, std::string{"200"} );
	EXPECT_EQ( http2_server_test::header_recv_v[1].find("content-length")->second, std::to_string(body.size()) );
	EXPECT_EQ( http2_server_test::header_recv_v[1].find("content-type")->second, content_type );
	EXPECT_EQ( http2_server_test::header_recv_v[1].find("host")->second, host );
	
	EXPECT_EQ( http2_server_test::data_recv_v[3], body2 );
	EXPECT_EQ( http2_server_test::header_recv_v[3].find(":status")->second, std::string{"200"} );
	EXPECT_EQ( http2_server_test::header_recv_v[3].find("content-length")->second, std::to_string(body2.size()) );
	EXPECT_EQ( http2_server_test::header_recv_v[3].find("content-type")->second, content_type );
	EXPECT_EQ( http2_server_test::header_recv_v[3].find("host")->second, host );
}

TEST_F(http2_server_test, too_many_streams)
{
static const std::string body{"Ave client, dummy node says hello"};
	static const std::string content_type{"text/plain"};
	static const std::string host{"cristo.it"};
	
	static const std::string body2{"Ave client, dummy node says hello hello"};
	std::unique_ptr<Connection>  c = start_client();
	Connection* cnx =  c.get();
	cnx->test = this;

	int request_done = 0;
	_handler->on_request([&](auto conn, auto req, auto res)
	{
		req->on_finished([res, &request_done](auto req)
		{
			http::http_response r;
			r.protocol(http::proto_version::HTTP20);
			r.status(200);
			r.header("content-type", content_type);
			r.header("date", "Tue, 17 May 2016 14:53:09 GMT");
			r.hostname(host);
			if ( request_done == 0 )
			{
				r.content_len(body.size());
				res->headers(std::move(r));
				res->body(make_data_ptr(body), body.size());
				++request_done;
			}
			else
			{
				r.content_len(body2.size());
				res->headers(std::move(r));
				res->body(make_data_ptr(body2), body2.size());
			}
			res->end();
		});
	});
	
	auto keep_alive = std::make_unique<boost::asio::io_service::work>(mock_connector->io_service());
	bool terminated{false};
	std::function<void()> io_poll;
	io_poll = [&io_poll, &keep_alive, cnx, &terminated, this] 
	{
		if (nghttp2_session_want_read(cnx->session) ||
			nghttp2_session_want_write(cnx->session))
		{
			exec_io( cnx );
			mock_connector->read( request_raw );
			request_raw = "";
			mock_connector->io_service().post(io_poll);
		}
		else
		{
			terminated = true;
			keep_alive.reset();
		}
	};
	mock_connector->io_service().post([this, &terminated, cnx, &io_poll]()
	{
		Request req;
		Request* greq = &req;
		greq->path = "/";
		greq->stream_id = -1;
		greq->hostport = "80";

		submit_settings(cnx);

		submit_request(cnx, greq);
		
		Request req2;
		Request* greq2 = &req2;
		greq2->path = "/second";
		greq2->stream_id = -1;
		greq2->hostport = "80";
		
		for ( int i = 0; i < 104; ++i )
			submit_request(cnx, greq2);

		io_poll();
	});
	mock_connector->io_service().run();
	nghttp2_session_del( cnx->session );

	ASSERT_TRUE( terminated );
	EXPECT_EQ( http2_server_test::stream_terminated, 105 );
	EXPECT_EQ( http2_server_test::data_recv, 100 );
	EXPECT_EQ( http2_server_test::header_recv, 100 );
	EXPECT_EQ( http2_server_test::closing_error_code,  NGHTTP2_REFUSED_STREAM );
}

TEST_F(http2_server_test, chunked)
{
	static const std::string body{"Ave client, dummy node says hello"};
	static const std::string content_type{"text/plain"};
	static const std::string host{"cristo.it"};
	
	int times =  1 + 16384 / body.size();
	std::size_t s = body.size() * times;
	
	std::string exp;
	for ( int i = 0; i < times; ++i )
		exp += body;
			
	std::unique_ptr<Connection>  c = start_client();
	Connection* cnx =  c.get();
	cnx->test = this;

	_handler->on_request([&](auto conn, auto req, auto res)
	{
		req->on_finished([res, times, s](auto req)
		{
			http::http_response r;
			r.protocol(http::proto_version::HTTP20);
			r.status(200);
			r.header("content-type", content_type);
			r.header("date", "Tue, 17 May 2016 14:53:09 GMT");
			r.hostname(host);

			r.content_len(s);
			res->headers(std::move(r));
			for ( int i = 0; i < times; ++i )
				res->body(make_data_ptr(body), body.size());
			res->end();
		});
	});
	
	auto keep_alive = std::make_unique<boost::asio::io_service::work>(mock_connector->io_service());
	bool terminated{false};
	std::function<void()> io_poll;
	io_poll = [&io_poll, &keep_alive, cnx, &terminated, this] 
	{
		if (nghttp2_session_want_read(cnx->session) ||
			nghttp2_session_want_write(cnx->session))
		{
			exec_io( cnx );
			mock_connector->read( request_raw );
			request_raw = "";
			mock_connector->io_service().post(io_poll);
		}
		else
		{
			terminated = true;
			keep_alive.reset();
		}
	};
	mock_connector->io_service().post([this, &terminated, cnx, &io_poll]()
	{
		Request req;
		Request* greq = &req;
		greq->path = "/";
		greq->stream_id = -1;
		greq->hostport = "80";

		submit_settings(cnx);

		submit_request(cnx, greq);

		io_poll();
	});
	mock_connector->io_service().run();
	nghttp2_session_del( cnx->session );

	ASSERT_TRUE( terminated );
	EXPECT_EQ( http2_server_test::stream_terminated, 1 );
	EXPECT_EQ( http2_server_test::data_recv, 498 );
	EXPECT_EQ( http2_server_test::header_recv, 1 );
	EXPECT_EQ( http2_server_test::closing_error_code, NGHTTP2_NO_ERROR );
	EXPECT_EQ( http2_server_test::data_recv_v[1], exp );
	EXPECT_EQ( http2_server_test::header_recv_v[1].find(":status")->second, std::string{"200"} );
	EXPECT_EQ( http2_server_test::header_recv_v[1].find("content-length")->second, std::to_string(s) );
	EXPECT_EQ( http2_server_test::header_recv_v[1].find("content-type")->second, content_type );
	EXPECT_EQ( http2_server_test::header_recv_v[1].find("host")->second, host );
}

TEST_F(http2_server_test, randomdata)
{
	const char raw_request[] = 
		"\x00\x05\x06\x04\x00\x00\x00\x00\x00\x00\x03\x00\x00\x00\x64\x50"
		"\x52\x49\x20\x2A\x20\x48\x54\x54\x50\x2F\x32\x2E\x30\x0D\x0A\x0D"
		"\x00\x00\x00\x00\x00\x00\x00\x00";
	std::size_t len = sizeof( raw_request );
		
	_handler = std::make_shared<http2::session>();
	
	_handler->start();
	bool error = _handler->on_read(raw_request, len);
	ASSERT_FALSE( error );
}

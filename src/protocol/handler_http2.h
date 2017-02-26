#pragma once

#include <list>
#include <memory>
#include <functional>
#include <boost/array.hpp>
#include <boost/asio.hpp>

#include "handler_factory.h"
#include "stream_handler.h"

#include "../utils/log_wrapper.h"
#include "../chain_of_responsibility/node_interface.h"
#include "../chain_of_responsibility/chain_of_responsibility.h"

namespace nghttp2{namespace asio_http2{ namespace server{
	struct http2_handler;
	struct serve_mux;
	struct request;
}}}

namespace server
{

class handler_http2 : public handler_interface
{
	using ng_h2h = nghttp2::asio_http2::server::http2_handler;
	using ng_mux = nghttp2::asio_http2::server::serve_mux;
	using outbuf = boost::array<uint8_t, 65536>;
	using inbuf = boost::array<uint8_t, MAXINBYTESPERLOOP>;
	using stream = std::shared_ptr<stream_handler>;

	bool local_should_stop{false};

	std::unique_ptr<ng_h2h> handler;
	std::unique_ptr<ng_mux> mux;
	std::list<stream> streams;

	// TODO: these boost array are used to copy data to be passed to the nghttp2 lib; they must go!
	outbuf _outbuf;
	inbuf _inbuf;

	void remove_terminated();

protected:
	void do_write() override;
	void on_connector_nulled() override;

public:
	//adapt constructor to take no parameters
	handler_http2();
	~handler_http2();

	bool start() noexcept override;
	bool should_stop() const noexcept override;
	bool on_read(const char*, size_t) override;
	bool on_write(dstring&) override;

	void on_eom() override;
	void on_error(const int &) override;

	errors::error_code error_code_distruction;
};
}

#include "perf_client.h"


#include <string>
#include <functional>
#include <utility>
#include <numeric>
#include <array>
#include <iostream>


namespace doormat {
namespace performance_test {

static const auto max_length = 1024;


perf_client::perf_client(const std::string& host,
						 uint32_t port,
						 const std::string& request,
						 size_t response_bytes,
						 size_t total_conns,
						 size_t reqs_per_conn)
	: host_(host)
	, port_(port)
	, resolver_(io_)
	, request_(request)
	, n_resp_bytes_(response_bytes)
	, total_conns_(total_conns)
	, reqs_per_conn_(reqs_per_conn)
	, buf_(response_bytes)
{
/*	dur_connection_.reserve(total_conns);
	dur_connect_.reserve(total_conns);
	dur_reply_.reserve(total_conns);*/
}

void perf_client::run()
{
	for(size_t i_conn = 0; i_conn < total_conns_; ++i_conn)
	{
		for(size_t i_req = 0; i_req < reqs_per_conn_; ++i_req)
		{
			reset();
			auto tp_start = clock::now();
			connect();
//			dur_connect_.emplace_back(clock::now() - tp_start);
            connection_.put(std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - tp_start).count());
			write();
			auto tp_written = clock::now();
			read(n_resp_bytes_);
			auto tp_done = clock::now();
            reply_.put(std::chrono::duration_cast<std::chrono::milliseconds>(tp_done - tp_written).count());
//			dur_reply_.emplace_back(tp_done - tp_written);
            connection_time_.put(std::chrono::duration_cast<std::chrono::milliseconds>(tp_done - tp_start).count());
//			dur_connection_.emplace_back(tp_done - tp_start);
			shutdown();
		}
	}
}

void perf_client::get_results() const
{
	using namespace std::chrono;
/*
	auto connect_avg = std::accumulate(dur_connect_.begin(), dur_connect_.end(), duration(0)).count() / 1000000. / dur_connect_.size();
    connect
	auto reply_avg = std::accumulate(dur_reply_.begin(), dur_reply_.end(), duration(0)).count() / 1000000. / dur_reply_.size();
	auto connection_avg = std::accumulate(dur_connection_.begin(), dur_connection_.end(), duration(0)).count() / 1000000. / dur_connection_.size();
	std::clog << "\n======= results =======\n"
			  << "connections: " << total_conns_ << '\n'
			  << "connect [ms]: " << connect_avg << '\n'
			  << "reply time [ms]: " << reply_avg << '\n'
			  << "connection time [ms]: " << connection_avg << '\n'
			  << "========================\n";*/
    connection_.push_average();
    reply_.push_average();
    connection_time_.push_average();
}

void perf_client::reset()
{
	socket_.reset(new ssl_socket(io_, ctx_));

	socket_->set_verify_mode(boost::asio::ssl::verify_none); // fixme: could not make it work with verify_peer
	socket_->set_verify_callback(
				std::bind(&perf_client::verify_certificate, this, std::placeholders::_1, std::placeholders::_2));
}

void perf_client::connect()
{
	boost::asio::ip::tcp::resolver::query query(host_, std::to_string(port_));
	auto endpoint_iterator = resolver_.resolve(query);

	auto it = boost::asio::connect(socket_->lowest_layer(), endpoint_iterator);
	if(it == decltype(resolver_)::iterator{})
		throw 1;

	socket_->lowest_layer().set_option(boost::asio::ip::tcp::no_delay(true));

	// fixme: connector_performance.first_byte test throws here from the second iteration on
	socket_->handshake(boost::asio::ssl::stream_base::client);

}


bool perf_client::verify_certificate(bool preverified, boost::asio::ssl::verify_context& ctx)
{
	// The verify callback can be used to check whether the certificate that is
	// being presented is valid for the peer. For example, RFC 2818 describes
	// the steps involved in doing this for HTTPS. Consult the OpenSSL
	// documentation for more details. Note that the callback is called once
	// for each certificate in the certificate chain, starting from the root
	// certificate authority.

	// In this example we will simply print the certificate's subject name.
	char subject_name[256];
	X509* cert = X509_STORE_CTX_get_current_cert(ctx.native_handle());
	X509_NAME_oneline(X509_get_subject_name(cert), subject_name, 256);

	return preverified;
}


void perf_client::write()
{
	auto n_written = boost::asio::write(*socket_, boost::asio::buffer(request_, request_.length()));
	if(n_written != request_.length())
		throw 1;
}

void perf_client::read(size_t bytes_to_read)
{
	auto n_read = boost::asio::read(*socket_, boost::asio::buffer(buf_));
	if(n_read != bytes_to_read)
		throw 1;
}

void perf_client::shutdown() noexcept
{
	boost::system::error_code ec;
	socket_->lowest_layer().cancel(ec);
	socket_->lowest_layer().shutdown(boost::asio::socket_base::shutdown_both, ec);
	socket_->shutdown(ec);
}




}
}

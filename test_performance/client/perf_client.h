#include "boost/asio.hpp"
#include "boost/asio/ssl.hpp"
#include "../utils/measurements.h"
#include <string>
#include <vector>
#include <chrono>
#include <memory>


namespace doormat {

namespace performance_test {


// from http://www.boost.org/doc/libs/1_47_0/doc/html/boost_asio/example/ssl/client.cpp


class perf_client
{
public:
	using clock = std::chrono::steady_clock;
	using time_point = clock::time_point;
	using duration = clock::duration;
	using ssl_socket = boost::asio::ssl::stream<boost::asio::ip::tcp::socket>;

	perf_client(const std::string& host,
				uint32_t port,
				const std::string& request,
				size_t response_bytes,
				size_t total_conns = 1,
				size_t reqs_per_conn = 1);

	void run();
	void get_results() const;

private:
	void reset();
	void connect();
	bool verify_certificate(bool preverified,
							boost::asio::ssl::verify_context& ctx);
	void write();
	void read(size_t bytes_to_read);
	void shutdown() noexcept;

	const std::string host_;
	const uint32_t port_;
	boost::asio::ssl::context ctx_{boost::asio::ssl::context::tlsv12};
	boost::asio::io_service io_;
	boost::asio::ip::tcp::resolver resolver_;
	std::unique_ptr<ssl_socket> socket_;
	std::string request_;
	const size_t n_resp_bytes_;
	const size_t total_conns_;
	const size_t reqs_per_conn_;
	std::vector<uint8_t> buf_;

	std::vector<duration> dur_connection_;
	std::vector<duration> dur_connect_;
	std::vector<duration> dur_reply_;


    measurement connection_{"Connect (ms)"};
    measurement reply_{"Reply (ms)"};
    measurement connection_time_{"Connection time (ms)"};


};



}

}

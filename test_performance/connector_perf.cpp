#include <gtest/gtest.h>

#include <string>
#include <functional>
#include <utility>
#include <chrono>
#include <vector>
#include <numeric>
#include <boost/asio.hpp>

#include "connector.h"
#include "utils/sni_solver.h"
#include "service_locator/service_initializer.h"
#include "service_locator/service_locator.h"
#include "stats/stats_manager.h"

#include "client/perf_client.h"

using namespace server;

namespace
{


const std::string crlf = "\r\n";

std::string operator "" _my_s(const char *str, std::size_t len)
{
	return std::string(str, len);
}

class mock_read_handler : public handler_interface
{
public:
	using time_point = std::chrono::high_resolution_clock::time_point;
	using work_ptr = std::unique_ptr<boost::asio::io_service::work>;

	mock_read_handler(size_t expected_bytes, work_ptr& work)
		: tot_bytes_read(expected_bytes)
		, work(work)
	{}

	// stops after the expected amount of bytes has been read
	bool on_read(const char*, size_t len) override
	{
		bytes_read += len;
		if(bytes_read == tot_bytes_read)
		{
			connector()->do_write();
		}

		return true;
	}

	// dummy functions
	bool start() noexcept override { return true; }

	bool should_stop() const noexcept override { return response_chunk >= default_response().size(); }
	bool on_write(dstring& chunk) override
	{
		if(response_chunk >= default_response().size())
		{
			work.reset();
			chunk = dstring{};
		}
		else
		{
			chunk = default_response()[response_chunk].c_str();
		}

		++response_chunk;
		return true;
	}

	void on_eom() override {}
	void on_error(const int&) override {}
// 	void on_response_continue() override {}

	static const std::vector<std::string>& default_response()
	{

		static const std::vector<std::string> ret = {
			"HTTP/1.1", crlf, "2", "0", "0", crlf, "OK", crlf, "Connection", " : ", "keep-alive", crlf,
			"Content-Length", " : ", "1", "7", crlf, "content-type", " : ", "application/json", crlf,
			"Date", " : ", "Fri, 30 Sep 2016 07:40:00 GMT", crlf, "Server", " : ", "Door-mat", crlf,
			"Via", " : ", "HTTP/1.1", crlf, crlf, R"({"hello":"world"})", crlf
		};

		return ret;
	}

protected:
	void do_write() override {}
	void on_connector_nulled() override
	{
		work.reset();
	}

private:
	const size_t tot_bytes_read;
	size_t bytes_read{};
	work_ptr& work;
	size_t response_chunk{};
};


}

using namespace doormat::performance_test;

TEST(connector_performance, whole_response)
{
	// load configuration and setup allocator
	service::initializer::load_configuration("../resources/doormat.test.config");
	service::initializer::init_services();
	std::string hosttest = "127.0.0.1";
	log_wrapper::init( false, "fatal", service::locator::configuration().get_log_path());

	const auto number_of_connections = 50;
	const std::string request = "GET /ciao HTTP/1.1"_my_s + crlf +
			"User-Agent: httperf/0.9.0"_my_s + crlf +
			"Host: localhost"_my_s + crlf + crlf;

	const auto port = 2443;
	const auto ip = boost::asio::ip::address_v4(std::array<uint8_t,4>{127,0,0,1});
	const auto endpoint = boost::asio::ip::tcp::endpoint(ip, port);

	// server: setup acceptor for the server side
	boost::asio::io_service io;
	std::unique_ptr<boost::asio::io_service::work> work;
	boost::asio::ip::tcp::acceptor acceptor(io);
	acceptor.open(endpoint.protocol());
	acceptor.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
	acceptor.bind(endpoint);
	acceptor.listen();

	// client
	std::thread th_client([&]
	{
		auto resp_bytes = std::accumulate(mock_read_handler::default_response().begin()
										  , mock_read_handler::default_response().end(),
										  std::string()).size();

		perf_client cl(hosttest, port, request, resp_bytes, number_of_connections);
		cl.run();
		cl.get_results();
	});

	// server side ssl stuff
	ssl_utils::sni_solver sni;
	sni.load_certificates();
	auto& ssl_ctx = sni.begin()->context;

	// cycle on connections
	for(size_t i = 0; i < number_of_connections; ++i)
	{
		std::unique_ptr<handler_interface> handler(new mock_read_handler(request.length(), work));
		auto socket = std::make_shared<ssl_socket>(acceptor.get_io_service(), ssl_ctx);
		{ // dummy scope to let connector's shared_ptr die
			auto conn = std::make_shared<connector<ssl_socket>>(boost::posix_time::seconds(1), boost::posix_time::seconds(1), socket);
			conn->handler(handler.get());
			acceptor.async_accept(socket->lowest_layer(), [conn, i](const boost::system::error_code& ec)
			{
				ASSERT_FALSE(ec) << "connection n° " << i;
				auto handshake_cb = [conn](const boost::system::error_code& ec)
				{
					conn->start(true);
				};

				conn->handshake_countdown();
				conn->socket().async_handshake(boost::asio::ssl::stream_base::server, handshake_cb);
			});
		}

		work.reset(new boost::asio::io_service::work(io));
		io.run();
		io.reset();
	}

	th_client.join();
}


TEST(connector_performance, first_byte)
{
	// load configuration and setup allocator
	service::initializer::load_configuration("../resources/doormat.test.config");
	service::initializer::init_services();
	log_wrapper::init(false, "fatal", service::locator::configuration().get_log_path());

	const auto number_of_connections = 50;
	const std::string request = "GET /ciao HTTP/1.1"_my_s + crlf +
			"User-Agent: httperf/0.9.0"_my_s + crlf +
			"Host: localhost"_my_s + crlf + crlf;

	const auto port = 2443;
	const auto ip = boost::asio::ip::address_v4(std::array<uint8_t,4>{127,0,0,1});
	const auto endpoint = boost::asio::ip::tcp::endpoint(ip, port);

	// server: setup acceptor for the server side
	boost::asio::io_service io;
	std::unique_ptr<boost::asio::io_service::work> work;
	boost::asio::ip::tcp::acceptor acceptor(io);
	acceptor.open(endpoint.protocol());
	acceptor.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
	acceptor.bind(endpoint);
	acceptor.listen();

	// client
	std::thread th_client([&]{
		perf_client cl("127.0.0.1", port, request, 1, number_of_connections);
		cl.run();
		cl.get_results();
	});

	// server side ssl stuff
	ssl_utils::sni_solver sni;
	sni.load_certificates();
	auto& ssl_ctx = sni.begin()->context;

	// cycle on connections
	for(size_t i = 0; i < number_of_connections; ++i)
	{
		std::unique_ptr<handler_interface> handler(new mock_read_handler(request.length(), work));
		auto socket = std::make_shared<ssl_socket>(acceptor.get_io_service(), ssl_ctx);
		{ // dummy scope to let connector's shared_ptr die
			auto conn = std::make_shared<connector<ssl_socket>>(boost::posix_time::seconds(1), boost::posix_time::seconds(1), socket);
			conn->handler(handler.get());
			acceptor.async_accept(socket->lowest_layer(), [conn, i](const boost::system::error_code& ec)
			{
				ASSERT_FALSE(ec) << "connection n° " << i;
				auto handshake_cb = [conn](const boost::system::error_code& ec)
				{
					conn->start(false);
				};

				conn->handshake_countdown();
				conn->socket().async_handshake(boost::asio::ssl::stream_base::server, handshake_cb);
			});
		}

		work.reset(new boost::asio::io_service::work(io));
		io.run();
		io.reset();
	}

	th_client.join();
}

#include "mock_server.h"
#include "../../src/utils/log_wrapper.h"

#include <gtest/gtest.h>
#include <iostream>

using namespace  boost::asio::ip;

template<>
std::unique_ptr<boost::asio::ip::tcp::socket> mock_server<false>::make_socket(boost::asio::io_service& io)
{
	return std::make_unique<socket_t>(io);
}

template<>
std::unique_ptr<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>> mock_server<true>::make_socket(boost::asio::io_service& io)
{
	return std::make_unique<socket_t>(io, ctx);
}

template<bool ssl>
mock_server<ssl>::mock_server(uint16_t listening_port) :
	listening_port{listening_port},
	stopped{false},
	ctx{boost::asio::ssl::context::tlsv12}
{
	if(ssl)
	{
		ctx.set_options(
					boost::asio::ssl::context::default_workarounds
					| boost::asio::ssl::context::verify_none);
		ctx.set_password_callback([](auto&&, auto&&) -> std::string
		{
			return "pippo123";
		});
		ctx.use_certificate_chain_file("../resources/certificates/npn/newcert.pem");
		ctx.use_private_key_file("../resources/certificates/npn/newkey.pem", boost::asio::ssl::context::pem);
	}
}

template<>
void mock_server<false>::start(std::function<void()> on_start_function, bool once)
{
	auto&& ios = service::locator::service_pool().get_thread_io_service();
	auto endpoint = tcp::endpoint{address::from_string("::"), listening_port};

	acceptor = std::make_unique<tcp::acceptor>(ios, endpoint);
	socket = make_socket(ios);
	acceptor->async_accept(socket->lowest_layer(), [this, on_start_function, once](const boost::system::error_code &ec)
	{
		if(once) acceptor->close();
		if(ec)
			return;

		stopped = false;
		on_start_function();
	});
}

template<>
void mock_server<true>::start(std::function<void()> on_start_function, bool once)
{
	auto&& ios = service::locator::service_pool().get_thread_io_service();
	auto endpoint = tcp::endpoint{address::from_string("::"), listening_port};

	acceptor = std::make_unique<tcp::acceptor>(ios, endpoint);
	socket = make_socket(ios);
	acceptor->async_accept(socket->lowest_layer(), [this, on_start_function, once](const boost::system::error_code &ec)
	{
		if(ec)
			return;

		auto handshake_cb = [this, once, on_start_function](const boost::system::error_code &ec)
		{
			if(once)
				acceptor->close();

			if (!ec)
			{
				stopped = false;
				on_start_function();
			}
		};
		socket->async_handshake(boost::asio::ssl::stream_base::server, handshake_cb);
	});
}

template<bool ssl>
bool mock_server<ssl>::is_stopped() const noexcept
{
	return stopped;
}

template<bool ssl>
void mock_server<ssl>::stop()
{
	if(stopped)
		return;


	if(socket != nullptr)
	{
		socket->lowest_layer().close();
		socket.reset();
	}

	if(acceptor != nullptr)
	{
		acceptor.reset();
	}

	stopped = true;
	LOGTRACE("stopped as requested");
}


template<bool ssl>
void mock_server<ssl>::read(int amount, read_feedback rf)
{
	if(stopped)
		return;

	std::shared_ptr<std::vector<char>> buf = std::make_shared<std::vector<char>>(amount);
	boost::asio::async_read(*socket, boost::asio::buffer(buf->data(), amount),
		[this, buf, rf](const boost::system::error_code &ec, size_t length)
		{
			if(ec && ec != boost::system::errc::operation_canceled)
			{
				LOGDEBUG(" error in connection: ", ec.message());
				stop();
			}
			if(rf) rf(std::string{ buf->data(), length });
		}
	);
}

template<bool ssl>
void mock_server<ssl>::write(const std::string &what, write_feedback wf)
{
	if(stopped)
		return;

	std::shared_ptr<std::string> buf = std::make_shared<std::string>(what);
	boost::asio::async_write(*socket, boost::asio::buffer(buf->data(), buf->size()),
		[this, buf, wf](const boost::system::error_code& ec , size_t len)
		{
			ASSERT_FALSE(ec && ec != boost::system::errc::operation_canceled)<<ec.message();
			if(wf) wf(len);
		}
	);
}


// explicit instatiation
template class mock_server<false>;
template class mock_server<true>;


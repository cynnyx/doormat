#include "mock_server.h"
#include "../../src/utils/log_wrapper.h"

#include <gtest/gtest.h>
#include <iostream>

using namespace  boost::asio::ip;

void mock_server::start(std::function<void()> on_start_function, bool once)
{
	auto&& ios = service::locator::service_pool().get_thread_io_service();
	auto endpoint = tcp::endpoint{address::from_string("::"), listening_port};

	acceptor.reset( new tcp::acceptor{ios, endpoint} );
	socket.reset( new tcp::socket{ios} );
	acceptor->async_accept(*socket, [this, on_start_function, once](const boost::system::error_code &ec)
	{
		if(once) acceptor->close();
		if(ec)
			return;
		stopped = false;
		on_start_function();
	});
}

bool mock_server::is_stopped()
{
	return stopped;
}

void mock_server::stop()
{
	if(stopped)
		return;


	if(socket != nullptr)
	{
		socket->close();
		socket.reset();
	}

	if(acceptor != nullptr)
	{
		acceptor.reset();
	}

	stopped = true;
	LOGTRACE("stopped as requested");
}


void mock_server::read(int amount, read_feedback rf)
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

void mock_server::write(const std::string &what, write_feedback wf)
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





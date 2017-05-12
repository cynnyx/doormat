#pragma once

#include <memory>
#include <functional>
#include <type_traits>

#include <boost/array.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/noncopyable.hpp>
#include <boost/date_time/posix_time/posix_time_duration.hpp>

#include "protocol/handler_factory.h"
#include "utils/dstring.h"
#include "utils/reusable_buffer.h"
#include "utils/log_wrapper.h"
#include "protocol/http_handler.h"

namespace server
{
constexpr const size_t MAXINBYTESPERLOOP{8192};

using interval = boost::posix_time::time_duration;
using berror_code = boost::system::error_code;
using tcp_socket = boost::asio::ip::tcp::socket;
using ssl_socket = boost::asio::ssl::stream<tcp_socket>;

class connector_interface
{
public:
	connector_interface() = default;
	connector_interface(const connector_interface&) = delete;
	connector_interface(connector_interface&&) = default;
	virtual ~connector_interface() = default;
	virtual void do_read() = 0;
	virtual void do_write() = 0;
	virtual boost::asio::ip::address origin() const = 0;
	virtual bool is_ssl() const noexcept = 0;
	virtual void close()=0;
    virtual boost::asio::io_service & io_service() = 0;
	virtual void set_timeout(std::chrono::milliseconds) = 0;
protected:
	reusable_buffer<MAXINBYTESPERLOOP> _rb;
	dstring _out;
};

// http_handler will become a template!?
template <typename socket_type>
class connector final: public connector_interface,
	public std::enable_shared_from_this< connector<socket_type> >
{
	std::shared_ptr<socket_type> _socket;
	std::shared_ptr<http_handler> _handler{nullptr};

	interval _ttl;

	boost::asio::deadline_timer _timer;

	bool _writing {false};
	bool _stopped {false};

	void cancel_deadline() noexcept
	{
		LOGTRACE(this, " deadline canceled");
		berror_code err;
		_timer.cancel(err);
		if(err)
			LOGERROR(this," ", err.message());
	}

	void schedule_deadline( const interval &msec )
	{
		auto self = this->shared_from_this();
		_timer.expires_from_now(msec);
		_timer.async_wait([self](const berror_code&ec)
		{
			if(!ec)
			{
				LOGTRACE(self.get()," deadline has expired");
				self->_handler->trigger_timeout_event();
			}
			else if(ec != boost::system::errc::operation_canceled)
			{
				LOGERROR(self.get()," error on deadline:", ec.message());
			}
		});
	}

	void set_timeout(std::chrono::milliseconds ms)
	{
		_ttl = boost::posix_time::milliseconds{ms.count()};
	}

public:

	void close() override
	{
		stop();
	}
	/// Construct a connection with the given io_service.
	explicit connector(std::shared_ptr<socket_type> socket) noexcept
		: _socket(std::move(socket))
		, _ttl(boost::posix_time::milliseconds(0))
		, _timer(_socket->get_io_service())
	{
		LOGTRACE(this," constructor");
	}

	bool is_ssl() const noexcept override { return  std::is_same<socket_type, ssl_socket>::value; }

	~connector() noexcept
	{
		LOGTRACE(this," destructor start");
		if(_handler)
		{
			// http_handler's derived classes will get an on_connector_nulled() event
			_handler->connector(nullptr);
			_handler = nullptr;
		}
		stop();
		LOGTRACE(this," destructor end");
	}

	boost::asio::ip::address origin() const override
	{
		boost::asio::ip::tcp::endpoint ep = _socket->lowest_layer().remote_endpoint();
		return ep.address();
	}

	//TODO: DRM-200:this method is required by ng_h2 apis, remove it once they'll be gone
	socket_type& socket() noexcept { return *_socket; }

	void renew_ttl() { if(_ttl != boost::posix_time::milliseconds{0}) schedule_deadline(_ttl); }

	void handler( std::shared_ptr<http_handler> h )
	{
		_handler = h;
        //todo: make it private;
		_handler->connector( this->shared_from_this() );
	}

	void start( bool tcp_no_delay = false )
	{
		assert( _handler );
		_socket->lowest_layer().set_option( boost::asio::ip::tcp::no_delay(tcp_no_delay) );
		if (_handler->start())
			do_read();
	}

	template<typename T = socket_type,typename std::enable_if<std::is_same<T, ssl_socket>::value, int>::type = 0>
	void stop() noexcept
	{
		if(!_stopped)
		{
			LOGTRACE(this," stopping");
			berror_code ec;
			_stopped = true;
			_timer.cancel(ec);
			_socket->lowest_layer().cancel(ec);
			_socket->shutdown(ec); // Shutdown - does it cause a TCP RESET?
		}
	}

	template<typename T = socket_type,typename std::enable_if<std::is_same<T, tcp_socket>::value, int>::type = 0>
	void stop() noexcept
	{
		if(!_stopped)
		{
			LOGTRACE(this," stopping");
			berror_code ec;
			_stopped = true;
			_timer.cancel(ec);
			_socket->lowest_layer().cancel(ec);
			_socket->shutdown(boost::asio::socket_base::shutdown_both, ec);
		}
	}

	void do_read() override
	{
		if(_stopped)
			return;

		renew_ttl();
		LOGTRACE(this," triggered a read");

		auto self = this->shared_from_this();
		auto buf = _rb.reserve();
		_socket->async_read_some( boost::asio::mutable_buffers_1(buf),
			[self](const berror_code& ec, size_t bytes_transferred)
			{
				self->cancel_deadline();
				if(!ec)
				{
					LOGTRACE(self.get()," received:",bytes_transferred," Bytes");
					assert(bytes_transferred);

					auto tmp = self->_rb.produce(bytes_transferred);
					if( self->_handler->on_read(tmp, bytes_transferred) )
					{
						self->_rb.consume(tmp + bytes_transferred);
						LOGTRACE(self.get()," read succeded");
						self->do_read();
					}
					else
					{
						LOGDEBUG(self.get()," error on_read - read failed");
						self->stop();
					}
				}
				else if(ec != boost::system::errc::operation_canceled)
				{
					LOGERROR(self.get()," error during read: ", ec.message());
					self->stop();
				}
				else
				{
					LOGDEBUG(self.get()," read canceled");
				}
			});
	}

	void do_write() override
	{
		if (_writing || _stopped)
			return;

		_out = dstring{};
		if ( !_handler->on_write(_out) )
		{
			LOGDEBUG(this," error on_write - write failed");
			return;
		}

		if( !_out && _handler->should_stop() )
		{
			LOGDEBUG(this," nothing left to write, stopping");
			stop();
			return;
		}

		renew_ttl();

		if( !_out )
			return;

		LOGTRACE(this," triggered a write of ", _out.size(), " bytes");
		_writing = true;

		auto self = this->shared_from_this(); //Let the connector live inside the callback
		boost::asio::async_write(*_socket, boost::asio::buffer(_out.cdata(), _out.size()),
			[self](const berror_code& ec, size_t s)
			{
				self->cancel_deadline();
				self->_writing = false;
				if(!ec)
				{
					LOGDEBUG(self.get()," correctly wrote ", s," bytes");
					self->do_write();
				}
				else if(ec != boost::system::errc::operation_canceled)
				{
					LOGERROR(self.get()," error during write: ", ec.message());
					self->stop();
				}
				else
				{
					LOGTRACE(self.get()," write canceled");
				}
			}
		);
	}


    boost::asio::io_service& io_service() {
        return _socket->get_io_service();
    }
};

}

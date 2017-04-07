#ifndef DOORMAT_COMMUNICATOR_H
#define DOORMAT_COMMUNICATOR_H

#include <boost/asio.hpp>
#include <queue>
#include <functional>
#include <memory>
#include "../chain_of_responsibility/error_code.h"
#include "../utils/dstring.h"
#include "../utils/log_wrapper.h"
#include "../errors/error_codes.h"
#include "../utils/reusable_buffer.h"
#include "../service_locator/service_locator.h"
#include "../io_service_pool.h"
#include "../configuration/configuration_wrapper.h"

namespace network
{

/** \brief communicator class is responsible of managing the communication incoming and outgoing from other components */

template<class socket_t = boost::asio::ip::tcp::socket>
class communicator
{
public:
	using read_cb_t = std::function<void(const char *, size_t)>;
	using error_cb_t = std::function<void(errors::error_code)>;
	static constexpr size_t reusable_buffer_size = 8192;

	communicator(std::unique_ptr<socket_t> s, read_cb_t read_callback, error_cb_t errcb) :
		read_callback{std::move(read_callback)}, error_callback{std::move(errcb)}, socket{std::move(s)},
		board_timeout{(int64_t) service::locator::configuration().get_board_timeout()}, timeout{service::locator::service_pool().get_thread_io_service()}
	{
		assert(socket);
	}

	communicator(const communicator& c) = delete;

	communicator(communicator &&c) : board_timeout{c.board_timeout},  timeout{c.timeout.get_io_service()}
	{
		socket.swap(c.socket);
		std::swap(board_timeout, c.board_timeout);
		std::swap(queue, c.queue);
		std::swap(read_callback, c.read_callback);
		std::swap(error_callback, c.error_callback);
		std::swap(errcode, c.errcode);
		std::swap(_rb, c._rb);
	}

	communicator& operator=(const communicator &c) = delete;
	communicator& operator=(communicator &&c)
	{
		socket.swap(c.socket);
		std::swap(board_timeout, c.board_timeout);
		std::swap(queue, c.queue);
		std::swap(read_callback, c.read_callback);
		std::swap(error_callback, c.error_callback);
		std::swap(errcode, c.errcode);
		std::swap(_rb, c._rb);
		std::swap(board_timeout, c.board_timeout);
		return *this;
	}

	/** \brief enqueues a dstring so that it can be written on the socket.
	 *  \param data the data to be sent on the socket
	 * */
	void write(dstring&& data)
	{
		queue.emplace(std::move(data));
		perform_write();
	}

	/** \brief starts the communicator activity */
	void start() { return perform_read(); }

	/** \brief stops the communicator and closes the socket.
	 *  \param force if true stops also the write operations currently in progress.
	 *
	 * */
	void stop(bool force=false)
	{
		LOGTRACE("stopping ", (int) waiting_count);
		if ( stopping ) return;
		if(queue.empty() || force)
		{
			socket->close();
			timeout.cancel();
		}
		stopping = true;
		set_error(INTERNAL_ERROR(1)); //fixme: stopped from remote, for the moment it is 500;
	}

	~communicator() = default;
private:
	/** \brief performs an actual write on the socket
	 * */
	void perform_write()
	{
		//no multiple write: boost::asio doesn't guarantee correctness in case of multiple writes on the same socket.
		if(writing || stopping || queue.empty()) return;

		writing = true;
		schedule_timeout(); //renews the deadline, since we had another event.
		++waiting_count;
		LOGTRACE("write of size ", queue.front().size(), " has been scheduled");
		boost::asio::async_write(*socket, boost::asio::buffer(queue.front().cdata(), queue.front().size()), [this](const boost::system::error_code &ec, size_t size)
		{
			LOGTRACE("wrote ", size, "bytes with return status ", ec.message());
			queue.pop();
			handle_timeout();
			--waiting_count;
			writing = false;
			if(!ec)
			{
				perform_write();
				return;
			}
			else if(ec != boost::system::errc::operation_canceled)
			{
				LOGDEBUG("error while writing to the remote endpoint: ", ec.message(), "; this will trigger a communicator stop.");
				set_error(INTERNAL_ERROR_LONG(502));
			}
			manage_termination();
		});
	}

	/** \brief performs an actual read on the socket. */
	void perform_read()
	{
		if(stopping) return;
		schedule_timeout();
		++waiting_count;
		auto buf = _rb.reserve();
		LOGTRACE("read operation pending");
		socket->async_read_some(boost::asio::mutable_buffers_1(buf),
			[this](const boost::system::error_code &ec, size_t size) mutable
			{
				LOGTRACE("read ", size, " bytes");
				handle_timeout();
				--waiting_count;
				if(!ec)
				{
					auto tmp = _rb.produce(size);
					if(!stop_delivered) read_callback(tmp, size);
					_rb.consume(tmp + size);
					perform_read();
				}
				else if(ec != boost::system::errc::operation_canceled)
				{
					LOGDEBUG("error while reading from the remote endpoint: ", ec.message(), "; this will trigger a communicator stop");
					set_error(INTERNAL_ERROR_LONG(500));
				}
				manage_termination();
			});
	}


	void handle_timeout() noexcept
	{
		if(waiting_count > 1)
		{
			//I'm not alone in the waiting room.. renew me
			schedule_timeout();
		}
		else
		{
			boost::system::error_code err;
			timeout.cancel(err);
		}
	}

	void schedule_timeout()
	{
		++waiting_count;
		LOGTRACE("scheduling timeout");
		timeout.expires_from_now(board_timeout);
		timeout.async_wait([this](const boost::system::error_code &ec)
		{
			--waiting_count;
			LOGTRACE("timeout terminated with ec ", ec.message());
			if(!ec || (ec  && ec != boost::system::errc::operation_canceled))
			{
				LOGERROR("here we have an error: ", ec.message());
				set_error(INTERNAL_ERROR_LONG(errors::http_error_code::internal_server_error));
			}
			manage_termination();
		});
	}

	void manage_termination()
	{
		if(errcode != 0)
		{
			/** There was an error: abort everything*/
			stop(true);
		}
		if(waiting_count == 0 && errcode != 0)
		{
			stop_delivered = true;
			return error_callback(errcode);
		}

	}

	void set_error(errors::error_code ec) noexcept
	{
		if(errcode.code() == 0)
		{
			errcode = ec;
			boost::system::error_code bec;
			if(socket) socket->shutdown(boost::asio::socket_base::shutdown_type::shutdown_both, bec);
		}
	}

	std::queue<dstring> queue;
	read_cb_t read_callback;
	error_cb_t error_callback;
	bool writing{false}, stopping{false}, stop_delivered{false};
	std::unique_ptr<socket_t> socket{nullptr};
	uint8_t waiting_count{0};
	errors::error_code errcode;
	boost::posix_time::milliseconds board_timeout;
	boost::asio::deadline_timer timeout;
	reusable_buffer<reusable_buffer_size> _rb;
};

}
#endif //DOORMAT_COMMUNICATOR_H

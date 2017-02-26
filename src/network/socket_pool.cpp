#include "socket_pool.h"
#include "../service_locator/service_locator.h"
#include "../io_service_pool.h"
#include "../configuration/configuration_wrapper.h"
#include "../utils/log_wrapper.h"
#include "../constants.h"

namespace network
{
	const std::chrono::seconds socket_pool::socket_expiration{10};
	const std::chrono::seconds socket_pool::request_expiration{3};

	socket_pool::socket_pool(size_t starting_boards) :
			required_board_size{starting_boards},
			socketcb_expiration{service::locator::service_pool().get_thread_io_service()}
	{
		assert(starting_boards);
		for(size_t i = 0; i < starting_boards; ++i)
		{
			generate_socket();
		}
	}

	/** \brief Schedules a timer and a connect
	 * */
	void socket_pool::generate_socket()
	{
		//assert(socket_queue.size() <= required_board_size);
		if(stopping)
			return;

		using namespace service;
		using namespace boost::asio;
		using namespace std;
		auto addr = locator::destination_provider().retrieve_destination();
		if(!addr.is_valid())
		{
			LOGERROR("cannot retrieve a valid address from the board map; wait a while until some endpoints go out of the fault cache");
			return;
		}

		LOGTRACE("Trying to connect to ", addr.ipv6().to_string(), " port ", addr.port());

		//create shared_ptr with null deleter, allowing to move raw ptr to unique_ptr for the user.
		std::shared_ptr<ip::tcp::socket> socket(new ip::tcp::socket(locator::service_pool().get_thread_io_service()), 
			[](ip::tcp::socket *){});
		auto timer = std::make_shared<deadline_timer>(locator::service_pool().get_thread_io_service());

		timer->expires_from_now(boost::posix_time::milliseconds(locator::configuration().get_board_connection_timeout()));
		timer->async_wait([socket, addr, timer](const boost::system::error_code &ec) mutable
		{
			if(!ec) socket->cancel();
		});

		socket->async_connect(addr.endpoint(),
		[socket, addr, this, timer](const boost::system::error_code &ec) mutable
		{
			++connection_attempts;
			if(!ec)
			{
				timer->cancel();
				socket->lowest_layer().set_option(boost::asio::ip::tcp::no_delay(true));
				/** schedule my read*/
				if(!stopping)
				{
					LOGTRACE("scheduling a read on the newly acquired socket on ", addr.ipv6(), " port ", addr.port());
					boost::asio::async_read(*socket, boost::asio::buffer(&fake_read_buffer, 1), [socket, addr](const boost::system::error_code &ec, size_t size) mutable
					{
						if(ec != boost::system::errc::operation_canceled)
						{
							/** Something went very, very wrong. Just delete the socket.*/
							LOGDEBUG("connection with ", addr.ipv6(), " port ", addr.port(), " returned with error ", ec.message(), " so it is going to be deleted. ");
							delete socket.get();
							socket.reset();
						} else
							LOGDEBUG("socket monitoring read has been canceled: ", addr.ipv6(), " ", addr.port());
						//if the code is operation_canceled, we don't need to do anything; the socket has already been given to a client for the time
						//in which this callback executes.
					});
				}
				append_socket(socket, socket_expiration, addr);
				locator::destination_provider().destination_worked(addr);
				return;
			}
			if(ec == boost::system::errc::too_many_files_open || ec == boost::system::errc::too_many_files_open_in_system)
			{
				LOGERROR("error while establishing connection because too many files are open by the socket_pool: ", ec.message(), "[", connection_attempts ,"]");
				decrease_pool_size();
				socket->close();
				timer->cancel();
				delete socket.get();
				socket.reset();
				return;
			}

			LOGERROR("Signaling to destination provider that address ", addr.ipv6(), " port ",
				addr.port(), " failed with error ", ec.value(), ": ", ec.message() );
			socket->close();
			delete socket.get();
			socket.reset();
			locator::service_pool().get_thread_io_service().post([this](){generate_socket(); });
			if(ec == boost::system::errc::operation_canceled) return;
			locator::destination_provider().destination_failed(addr);
			timer->cancel();
			return;
		});
	}

	/** \brief inserts the socket in the system, either by returning it to pending users or 
	 * by enqueuing it in case no users are waiting.
	 *	\param socket unique_ptr to the connected socket
	 *	\param duration expiry time of the socket.
	 *	\param addr endpoint with which the socket has been opened.
	 * */
	void socket_pool::append_socket(std::shared_ptr<socket_type> socket, const std::chrono::seconds &duration, 
		const routing::abstract_destination_provider::address &addr)
	{
		//assert(socket_queue.size() <= required_board_size);

		if(anysocket_cb.size())
		{
			//just return it to the caller.
			auto &cb = anysocket_cb.front().first;
			socket->cancel();
			std::unique_ptr<socket_type> socket_ptr(socket.get());
			socket.reset();
			cb(std::move(socket_ptr));
			anysocket_cb.pop();
			renew_anysocket_timer();
			return;
		}
		if(stopping) return;
		auto bucket = socket_queues.find(addr);
		if(bucket != socket_queues.end()) bucket->second.emplace(std::move(socket), duration);
		else
		{
			auto d = std::queue<socket_representation>();
			d.emplace(socket_representation(std::move(socket), duration));
			socket_queues.emplace(addr, std::move(d));
		}
	}

	/** \brief gets a socket to a specifie destination.
	 * */
	void socket_pool::get_socket(const routing::abstract_destination_provider::address &address, socket_callback cb)
	{
		using namespace service;
		using namespace boost::asio;
		using namespace std;
		if(!address.is_valid() || stopping) 
			return service::locator::service_pool().get_thread_io_service().post(std::bind(cb, nullptr));
		auto addr_bucket = socket_queues.find(address);
		if(addr_bucket != socket_queues.end() && addr_bucket->second.size() && addr_bucket->second.front().is_valid())
		{
			/** Retrieve from bucket, because we already have one. */
			auto socket = addr_bucket->second.front().socket.lock();
			if(!socket)
			{
				//pop one, then retry recursively; this will work like a charm.
				addr_bucket->second.pop();
				return get_socket(address, std::move(cb));
			}
			socket->cancel(); //delete pending read used for monitoring purposes.
			auto ptr = socket.get();
			socket.reset();
			cb(std::unique_ptr<socket_type>(ptr));
			addr_bucket->second.pop();
			generate_socket();
			return;
		}
		std::shared_ptr<ip::tcp::socket> socket(new ip::tcp::socket(locator::service_pool().get_thread_io_service()), 
			[](ip::tcp::socket *){});
		auto timer = std::make_shared<deadline_timer>(locator::service_pool().get_thread_io_service());
		timer->expires_from_now(boost::posix_time::milliseconds(locator::configuration().get_board_connection_timeout()));
		timer->async_wait([socket, timer](const boost::system::error_code &ec) mutable
			{
				if(!ec)
				{
					socket->cancel();
				}
			});

		socket->async_connect(address.endpoint(),
			[socket, cb, address, this, timer](const boost::system::error_code &ec) mutable
			{
				if(!ec)
				{
					timer->cancel();
					boost::asio::ip::tcp::socket* socket_ptr = socket.get();
					socket.reset();
					socket_ptr->lowest_layer().set_option(boost::asio::ip::tcp::no_delay(true));
					cb(std::unique_ptr<boost::asio::ip::tcp::socket>(socket_ptr));
					locator::destination_provider().destination_worked(address);
					return;
				}
				LOGERROR("error while establishing connection: ", ec.message());
				if ( ec != boost::system::errc::too_many_files_open && ec != boost::system::errc::too_many_files_open ) 
					locator::destination_provider().destination_failed(address);

				cb(nullptr);
			});
	}

	void socket_pool::get_socket(socket_callback cb)
	{
		if(socket_queues.size())
		{
			auto random_addr = std::next(std::begin(socket_queues), rand() % socket_queues.size());

			auto &Q = random_addr->second;

			auto oldSize = Q.size();
			while (Q.size() && !Q.front().is_valid())
				Q.pop();

			if(oldSize > Q.size())
				decrease_pool_size();

			if (Q.size())
			{
				auto socket = Q.front().socket.lock();
				/*if(!socket) decide how to react
				{
					return get_socket(std::move(cb));
				}*/
				socket->cancel(); //delete pending read used for monitoring purposes.
				auto ptr = socket.get();
				socket.reset();
				cb(std::unique_ptr<socket_type>(ptr));
				Q.pop();
				generate_socket();
				return;
			}
			socket_queues.erase(random_addr);
			if (socket_queues.size()) return get_socket(std::move(cb));
			/**The first that finds it empty should increment the number of requested sockets... How do we know it? because anysocket_cb.size() is 0*/
			if (stopping) return;
		}
		auto expiration = std::chrono::system_clock::now() + request_expiration;
		if(anysocket_cb.empty())
		{
			for(size_t i = 0; i < required_board_size; ++i)
				generate_socket();
			increase_pool_size();
			socketcb_expiration.expires_from_now(boost::posix_time::seconds(request_expiration.count()));
			socketcb_expiration.async_wait([this](const boost::system::error_code &ec)
			{
				if(!ec)
				{
					notify_connect_timeout();
					renew_anysocket_timer();
					return;
				}
			});
		}
		anysocket_cb.emplace(std::make_pair(std::move(cb), expiration));
		generate_socket();
	}
	
	void socket_pool::get_socket( const http::http_request &req, socket_pool::socket_callback socket_callback ) 
	{
		get_socket(std::move(socket_callback));
	}

	void socket_pool::renew_anysocket_timer()
	{
		if(anysocket_cb.size())
		{
			auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(anysocket_cb.front().second - std::chrono::system_clock::now());
			socketcb_expiration.expires_from_now(boost::posix_time::milliseconds(duration.count()));
			socketcb_expiration.async_wait([this](const boost::system::error_code &ec)
			{
				if(!ec)
				{
					notify_connect_timeout();
					renew_anysocket_timer();
					return;
				}
			});
		} else
		{
			socketcb_expiration.cancel();
		}
	}


	void socket_pool::notify_connect_timeout()
	{
		if(anysocket_cb.empty()) return;
		auto &cb = anysocket_cb.front().first;
		cb(nullptr);
		anysocket_cb.pop();
	}

	void socket_pool::increase_pool_size()
	{
		static const uint upper_limit =
			0.5*(service::locator::configuration().get_fd_limit() - reserved_fds())/service::locator::configuration().get_thread_number();

		const auto oldsize = required_board_size;
		required_board_size *= 2;

		if( required_board_size > upper_limit )
			required_board_size = oldsize;

		LOGDEBUG("size has been raised to ", required_board_size, " from ", oldsize);
	}

	void socket_pool::decrease_pool_size()
	{
		if(required_board_size > 1)
		{
			required_board_size /= 2;
			LOGDEBUG("size has been reduced to ", required_board_size);
		}
	}

	socket_pool::socket_representation::socket_representation(std::weak_ptr<socket_type> socket,
																const std::chrono::seconds &duration) : socket{std::move(socket)}
	{
		expiration = std::chrono::system_clock::now() + duration;
	}


	void socket_pool::stop()
	{
		stopping = true;
		LOGTRACE("STOP! [", socket_queues.size(), "]");
		for(auto &b : socket_queues)
		{
			while(b.second.size())
			{
				if( auto ptr = b.second.front().socket.lock()) ptr->cancel();
				b.second.pop();
			}
		}


		socketcb_expiration.cancel();

	}

	socket_pool::~socket_pool()
	{
		while(anysocket_cb.size())
		{
			anysocket_cb.front().first(nullptr);
			anysocket_cb.pop();
		}

		for(auto &b : socket_queues)
		{
			while(b.second.size())
			{
				if( auto ptr = b.second.front().socket.lock()) ptr->cancel();
				b.second.pop();
			}
		}

	}
	
	std::unique_ptr<socket_factory> socket_pool_factory::get_socket_factory( std::size_t size ) const
	{
		return std::unique_ptr<socket_factory>{ new socket_pool{ size } };
	}

}

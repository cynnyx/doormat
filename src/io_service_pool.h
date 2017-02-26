#pragma once

#include <vector>
#include <memory>
#include <future>
#include <functional>
#include <boost/noncopyable.hpp>
#include <boost/asio/io_service.hpp>

namespace server
{

/// A pool of io_service objects.
class io_service_pool : private boost::noncopyable
{
	/// The pool of io_services.
	std::vector<std::unique_ptr<boost::asio::io_service>> io_services_;

	/// The work that keeps the io_services running.
	std::vector<std::unique_ptr<boost::asio::io_service::work>> work_;

	/// The next io_service to use for a connection.
	std::size_t next_io_service_;

	/// Futures to all the io_service objects
	std::vector<std::shared_future<void>> futures;

	std::size_t pool_size;

	/// Get access to all io_service objects.
	const std::vector<std::unique_ptr<boost::asio::io_service>>& io_services() const;

	/// Join on all io_service objects in the pool.
	void join();

public:
	using thread_init_fn_t = std::function<void(boost::asio::io_service&)>;
	using main_init_fn_t = std::function<void(void)>;

	/// Construct the io_service pool.
	explicit io_service_pool( std::size_t pool_size );

	/// Run all io_service objects in the pool.
	void run(thread_init_fn_t t_fn = {}, main_init_fn_t m_fn = {});

	/// Stop all io_service objects in the pool.
	void stop();

	/// Reset all io_service objects in the pool.
	void reset();

	/// Get the size of the service pool
	std::size_t size() const noexcept;

	/// Get an io_service to use.
	boost::asio::io_service& get_io_service();

	/// Get always the same io_service from within the same thread.
	boost::asio::io_service& get_thread_io_service() const;

	void allow_graceful_termination() noexcept;
};

} // namespace

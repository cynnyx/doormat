#include "io_service_pool.h"
#include "utils/log_wrapper.h"

#include <chrono>
#include <atomic>
#include <iostream>

namespace server
{

io_service_pool::io_service_pool( std::size_t pool_size ) : next_io_service_( 0 ), pool_size( pool_size )
{
	if ( pool_size == 0 )
		throw std::runtime_error( "io_service_pool size is 0" );

	// Give all the io_services work to do so that their run() functions will not
	// exit until they are explicitly stopped.

	for ( std::size_t i = 0; i < pool_size; ++i )
	{
		io_services_.emplace_back( new boost::asio::io_service() );
		work_.emplace_back( new boost::asio::io_service::work(*io_services_.back()) );
	}
}

void io_service_pool::allow_graceful_termination() noexcept
{
	work_.clear();
}

void io_service_pool::run(thread_init_fn_t t_fn, main_init_fn_t m_fn)
{
	auto thread_fn = [this, t_fn]
	{
		auto&& iosv = get_thread_io_service();
		try
		{
			if(t_fn)
				t_fn(iosv);
			iosv.run();
			iosv.reset();
		}
		catch(boost::system::error_code& ec)
		{
			LOGERROR(ec.message());
		}
	};

	//Create a brand new thread-pool to run all of the io_services.
	for ( std::size_t i = 0; i < io_services_.size(); ++i )
		futures.emplace_back( std::async(std::launch::async, thread_fn) );

	if(m_fn)
		m_fn();

	join();
}

void io_service_pool::join()
{
	// Wait for all threads in the pool to exit.
	for(auto& t : futures)
		if(t.valid())
			t.get();

	futures.clear();
}

void io_service_pool::stop()
{
	// Explicitly stop all io_services.
	for ( auto& iosv : io_services_ )
		iosv->stop();
}

void io_service_pool::reset()
{
	// Explicitly stop all io_services.
	for ( auto& iosv : io_services_ )
		iosv->reset();
}

std::size_t io_service_pool::size() const noexcept
{
	return pool_size;
}

boost::asio::io_service& io_service_pool::get_io_service()
{
	// Use a round-robin scheme to choose the next io_service to use.
	auto& io_service = *io_services_[next_io_service_];
	++next_io_service_;
	if ( next_io_service_ == io_services_.size() )
	{
		next_io_service_ = 0;
	}
	return io_service;
}

boost::asio::io_service& io_service_pool::get_thread_io_service() const
{
	// this is a workaround to link somehow a thread to a unique io_service
	// but nobody assures that worker threads are associated to different io_service's;
	// e.g. if we interleave a calla to get_thread_io_service() from the main thread
	// before we have called it from within all the working threads, we will associate
	// the first and the last working thread to the same io_services_[0]
	static std::atomic_size_t thread_counter;
	thread_local static const size_t thread_id = thread_counter++ % io_services().size();
	return *io_services()[thread_id];
}

const std::vector<std::unique_ptr<boost::asio::io_service>>&
io_service_pool::io_services() const
{
	return io_services_;
}
} // namespace

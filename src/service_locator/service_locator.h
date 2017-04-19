#pragma once

#include <memory>
#include <boost/asio.hpp>

namespace configuration
{
class configuration_wrapper;
}

namespace logging
{
class access_log;
class error_log;
class inspector_log;
}

namespace server
{
class io_service_pool;
}

namespace stats
{
class stats_manager;
}

namespace routing
{
struct abstract_destination_provider;
}

class fs_manager_wrapper;

namespace network
{
template<class T>
class socket_factory;
template<class T>
class abstract_factory_of_socket_factory;

class communicator_factory;
}

namespace endpoints
{
class chain_factory;
}
namespace service
{

class locator
{
	friend class initializer;

	static std::unique_ptr<configuration::configuration_wrapper> _configuration;
	static std::unique_ptr<server::io_service_pool> _service_pool;
	static std::unique_ptr<logging::access_log> _access_log;
	static thread_local std::unique_ptr<logging::inspector_log> _inspector_log;
	static std::unique_ptr<stats::stats_manager> _stats_manager;
	static std::unique_ptr<fs_manager_wrapper> _fsm;
	static std::unique_ptr<endpoints::chain_factory> chfac;
    static thread_local std::unique_ptr<network::communicator_factory> _communicator_factory;
	// Warning - this is not to be used any more, if not as a cache.
	template<class T = boost::asio::ip::tcp::socket>
	static thread_local std::unique_ptr<typename network::socket_factory<T>> _socket_pool; 
	// This factory must be thread safe
	template<class T = boost::asio::ip::tcp::socket>
	static std::unique_ptr<typename network::abstract_factory_of_socket_factory<T>> _socket_pool_factory;

public:
	/**
	 * @brief configuration returns a reference to the application wide instance of \ref configuration_wrapper.
	 * @throw \ref not_initialized if the object has not been initialized yet
	 */
	static configuration::configuration_wrapper& configuration() noexcept;

	/**
	 * @brief access_log
	 * @return a reference to a thread-safe instance of the access logger
	 */
	static logging::access_log& access_log() noexcept;

	/**
	 * @brief inspector_log
	 * @return a reference to a thread-safe instance of the error logger
	 */
	static logging::inspector_log& inspector_log() noexcept;

	/**
	 * @brief service_pool
	 * @return
	 */
	static server::io_service_pool& service_pool() noexcept;

	/**
	 * @brief stats_manager
	 * @return
	 */
	static stats::stats_manager& stats_manager() noexcept;
	
	static endpoints::chain_factory& chain_factory() noexcept;

	static network::communicator_factory& communicator_factory() noexcept;

	template<class T = boost::asio::ip::tcp::socket>
	static network::abstract_factory_of_socket_factory<T>& socket_pool_factory() noexcept
	{
		assert(_socket_pool_factory<T>);
		return *_socket_pool_factory<T>;
	}
};

template<class T>
std::unique_ptr<network::abstract_factory_of_socket_factory<T>> locator::_socket_pool_factory;
}

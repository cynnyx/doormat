#include "service_locator.h"
#include "../fs/fs_manager_wrapper.h"
#include "../configuration/configuration_wrapper.h"
#include "../io_service_pool.h"
#include "../log/log.h"
#include "../log/inspector_serializer.h"
#include "../board/abstract_destination_provider.h"
#include "../stats/stats_manager.h"
#include "../utils/log_wrapper.h"
#include "../network/socket_factory.h"

namespace service
{

std::unique_ptr<configuration::configuration_wrapper> locator::_configuration;
std::unique_ptr<server::io_service_pool> locator::_service_pool;
std::unique_ptr<logging::access_log> locator::_access_log;
thread_local std::unique_ptr<logging::inspector_log> locator::_inspector_log;
std::unique_ptr<routing::abstract_destination_provider> locator::_destination_provider;
std::unique_ptr<stats::stats_manager> locator::_stats_manager;
std::unique_ptr<fs_manager_wrapper> locator::_fsm;
thread_local std::unique_ptr<network::socket_factory> locator::_socket_pool;
std::unique_ptr<network::abstract_factory_of_socket_factory> locator::_socket_pool_factory;

configuration::configuration_wrapper& locator::configuration() noexcept
{
	assert(_configuration);
	return *_configuration;
}

server::io_service_pool& locator::service_pool() noexcept
{
	assert(_service_pool);
	return *_service_pool;
}

logging::access_log& locator::access_log() noexcept
{
	assert(_access_log);
	return *_access_log;
}

logging::inspector_log& locator::inspector_log() noexcept
{
	assert(_inspector_log);
	return *_inspector_log;
}

stats::stats_manager& locator::stats_manager() noexcept
{
	assert(_stats_manager);
	return *_stats_manager;
}

routing::abstract_destination_provider& locator::destination_provider() noexcept
{
	assert(_destination_provider);
	return *_destination_provider;
}

fs_manager_wrapper& locator::fs_manager() noexcept
{
	assert(_fsm);
	return *_fsm;
}

network::socket_factory& locator::socket_pool() noexcept
{
	assert(_socket_pool);
	return *_socket_pool;
}

network::abstract_factory_of_socket_factory& locator::socket_pool_factory() noexcept
{
	assert(_socket_pool_factory);
	return *_socket_pool_factory;
}

}

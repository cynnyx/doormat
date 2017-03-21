#include "service_locator.h"
#include "../configuration/configuration_wrapper.h"
#include "../io_service_pool.h"
#include "../log/log.h"
#include "../log/inspector_serializer.h"
#include "../stats/stats_manager.h"
#include "../utils/log_wrapper.h"
#include "../network/socket_factory.h"

namespace service
{

std::unique_ptr<configuration::configuration_wrapper> locator::_configuration;
std::unique_ptr<server::io_service_pool> locator::_service_pool;
std::unique_ptr<logging::access_log> locator::_access_log;
thread_local std::unique_ptr<logging::inspector_log> locator::_inspector_log;
std::unique_ptr<stats::stats_manager> locator::_stats_manager;

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


}

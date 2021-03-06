#include "log_wrapper.h"

#include <atomic>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/attributes/mutable_constant.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/attributes/mutable_constant.hpp>
#include <boost/date_time/posix_time/posix_time_types.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/file_sinks.h>
#include <new>         // placement new
#include <type_traits> // aligned_storage


namespace log_wrapper
{

namespace details
{

struct logger_stuff
{
	std::shared_ptr<spdlog::logger> logger_;
	std::shared_ptr<spdlog::logger> console_logger_;
	std::string log_filename_;
};

static size_t nifty_counter{};
static std::aligned_storage_t<sizeof(logger_stuff), alignof(logger_stuff)> raw_storage;
static logger_stuff& stuff = reinterpret_cast<logger_stuff&>(raw_storage);

logger_nifty_counter::logger_nifty_counter()
{
	if(nifty_counter++ == 0)
		new (&stuff) logger_stuff{};
}

logger_nifty_counter::~logger_nifty_counter()
{
	if(--nifty_counter == 0)
		(&stuff)->~logger_stuff();
}

inline std::shared_ptr<spdlog::logger>& _logger()
{
	return stuff.logger_;
}

inline std::shared_ptr<spdlog::logger>& _console_logger()
{
	return stuff.console_logger_;
}

inline std::string& _log_filename()
{
	return stuff.log_filename_;
}

} // namespace details

void init(bool verbose, const std::string& level, const std::string& folder)
{
	namespace spd = spdlog;
	spdlog::set_async_mode(32768, spdlog::async_overflow_policy::block_retry);
	details::_log_filename() = folder+"doormat_server_0.log";
	details::_logger() = spdlog::basic_logger_mt("_logger", folder+"doormat_server_0.log");
	details::_console_logger() = spdlog::stderr_logger_mt("_console");
	spd::level::level_enum lvl = spd::level::off;
	if(level == "trace")
	{
		lvl = spd::level::trace; // Set specific logger's log level
	} else if(level == "debug")
	{
		lvl = spd::level::debug;
	} else if(level == "warning")
	{
		lvl = spd::level::warn;
	} else if(level == "error")
	{
		lvl = spd::level::err;
	} else if(level == "fatal")
	{
		lvl = spd::level::critical;
	}
	//logging::add_common_attributes();
	details::_logger()->set_level(lvl);
	if(verbose)
		details::_console_logger()->set_level(spd::level::trace);
	else
		details::_console_logger()->set_level(lvl);
	/** Add log format to spdlog.*/
	//Set log format

	spdlog::set_pattern("%Y-%m-%d_%H:%M:%S.%f\t[%l]\t[%t] %v");
	details::_logger()->flush_on(spd::level::critical);
	details::_console_logger()->flush_on(spd::level::critical);

#ifdef NDEBUG
	details::_console_logger()->info("Doormat ", VERSION);
#else
	details::_console_logger()->info("Doormat ", VERSION, " Debug");
#endif

}

void rotate()
{
	if(!details::_logger()) return;

	details::_logger()->flush();
	details::_logger() =spdlog::basic_logger_mt("_logger", details::_log_filename());
}


	void log_trace(const std::string&s){
		details::_logger()->trace(s);
		details::_console_logger()->trace(s);
	}

	void log_debug(const std::string&s)
	{
		details::_logger()->debug(s);
		details::_console_logger()->debug(s);
	}
	void log_info(const std::string&s)
	{
		details::_logger()->info(s);
		details::_console_logger()->info(s);
	}
	void log_warn(const std::string& s)
	{
		details::_logger()->warn(s);
		details::_console_logger()->warn(s);
	}
	void log_error(const std::string&s)
	{
		details::_logger()->error(s);
		details::_console_logger()->error(s);
	}
	void log_fatal(const std::string&s)
	{
		details::_logger()->critical(s);
		details::_console_logger()->critical(s);
	}
}


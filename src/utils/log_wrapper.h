#pragma once

#include <boost/log/trivial.hpp>
#include "utils.h"

#define _SHORT_FILE_ (log_wrapper::details::past_last_slash(__FILE__))
#define LOGTRACE(...) log_wrapper::log_trace(_SHORT_FILE_, __LINE__, __VA_ARGS__)
#define LOGDEBUG(...) log_wrapper::log_debug(_SHORT_FILE_, __LINE__, __VA_ARGS__)
#define LOGINFO(...) log_wrapper::log_info(_SHORT_FILE_, __LINE__, __VA_ARGS__)
#define LOGWARN(...) log_wrapper::log_warning(_SHORT_FILE_, __LINE__, __VA_ARGS__)
#define LOGERROR(...) log_wrapper::log_error(_SHORT_FILE_, __LINE__, __VA_ARGS__)
#define LOGFATAL(...) log_wrapper::log_fatal(_SHORT_FILE_, __LINE__, __VA_ARGS__)

namespace spdlog {
	class logger;
}

namespace log_wrapper
{

namespace details
{
	static constexpr const char* const past_last_slash(const char* const str, const char* const last_slash)
	{
		return *str == '\0' ? last_slash : *str == '/'  ? past_last_slash(str + 1, str + 1) : past_last_slash(str + 1, last_slash);
	}

	static constexpr const char* const past_last_slash(const char* const str)
	{
		return past_last_slash(str, str);
	}

	// The nifty counter prevents the static initialization order fiasco
	// https://en.wikibooks.org/wiki/More_C++_Idioms/Nifty_Counter
	struct logger_nifty_counter
	{
		logger_nifty_counter();
		~logger_nifty_counter();
	};

	static logger_nifty_counter logger_schwarz_counter;
}

/** \brief Sets up the logging system. Its behaviour will be different depending on whether it is in release mode or not.
	 * \param logLevel string representation of the minimum logging level. Logging levels are, in increasing order. trace, debug, info, warning, error, fatal+
	 *  \param loggingDirectory: where to place the logs.
	 */
void init(bool verbose=false, const std::string& level = "info", const std::string& loggingDirectory="");
void rotate();
void log_trace(const std::string&);
void log_debug(const std::string&);
void log_info(const std::string&);
void log_warn(const std::string&);
void log_error(const std::string&);
void log_fatal(const std::string&);

template<typename... Args>
void log_trace(const char* const file, uint line, Args&&... args)
{
	log_trace(utils::stringify("[", file , ":", line ,"] ", std::forward<Args>(args)...));
}


template<typename... Args>
void log_debug(const char* const file, uint line, Args&&... args)
{
	log_debug(utils::stringify("[", file , ":", line ,"] ", std::forward<Args>(args)...));
}

template<typename... Args>
void log_info(const char* const file, uint line, Args&&... args)
{
	log_info(utils::stringify("[", file , ":", line ,"] ", std::forward<Args>(args)...));
}


template<typename... Args>
void log_warning(const char* const file, uint line, Args&&... args)
{
	log_warn(utils::stringify("[", file , ":", line ,"] ", std::forward<Args>(args)...));
}

template<typename... Args>
void log_error(const char* const file, uint line, Args&&... args)
{
	log_error(utils::stringify("[", file , ":", line ,"] ", std::forward<Args>(args)...));
}

template<typename... Args>
void log_fatal(const char* const file, uint line, Args&&... args)
{
	log_fatal(utils::stringify("[", file , ":", line ,"] ", std::forward<Args>(args)...));
}

}



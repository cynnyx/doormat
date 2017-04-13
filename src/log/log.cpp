#include "log.h"
#include "format.h"
#include "../constants.h"
#include "../service_locator/service_locator.h"
#include "../configuration/configuration_wrapper.h"
#include "../utils/null_deleter.h"

#include <sstream>
#include <fstream>
#include <chrono>
#include <cstdio>

#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/sources/channel_logger.hpp>
#include <boost/log/sources/severity_channel_logger.hpp>
#include <boost/log/attributes/function.hpp>
#include <boost/log/attributes/current_thread_id.hpp>
#include <boost/log/attributes/current_process_id.hpp>
#include <boost/log/attributes/mutable_constant.hpp>
#include <boost/log/expressions/formatters/date_time.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/log/support/date_time.hpp>

/*
 * Thread-safety notes:
 * - if you want to use a thread-safe sink frontend (in the sense that it takes
 * care of synchronizing the access to the backend), use the synchronous_sink
 * - as far as I can see, all boost-log sink backends need synchronization
 * in the frontend, i.e. does not implement synchronization on their own
 * - if you want to access to a backend in a thread-safe manner, you
 * can get it through the locked_backend member function, provided by
 * all sink frontends
 * -  there are two versions of loggers provided by the library: the thread-safe ones
 * and the non-thread-safe ones. For the non-thread-safe loggers it is safe for different
 * threads to write logs through different instances of loggers and thus there should
 * be a separate logger for each thread that writes logs. The thread-safe counterparts
 * can be accessed from different threads concurrently, but this will involve locking and
 * may slow things down in case of intense logging. The thread-safe logger types
 * have the _mt suffix in their name.
 *
 * - boost is not letting me using the unlocked_sink with the text_ostream_backend!!!!
 */

namespace
{
static constexpr const char* hypen = "-";
static constexpr const char* p = "p";
static constexpr const char* dot = ".";
}

namespace logging
{

BOOST_LOG_ATTRIBUTE_KEYWORD(timestamp_attribute, "TimeStamp", std::string)

BOOST_LOG_ATTRIBUTE_KEYWORD(hostname_attribute, "HostName", std::string)
BOOST_LOG_ATTRIBUTE_KEYWORD(logname_attribute, "LogName", std::string)
BOOST_LOG_ATTRIBUTE_KEYWORD(user_attribute, "User", std::string)
BOOST_LOG_ATTRIBUTE_KEYWORD(req_attribute, "Req1stLine", std::string)
BOOST_LOG_ATTRIBUTE_KEYWORD(status_attribute, "Status", int)
BOOST_LOG_ATTRIBUTE_KEYWORD(size_attribute, "ResponseSize", size_t)
BOOST_LOG_ATTRIBUTE_KEYWORD(referrer_attribute, "Referrer", std::string)
BOOST_LOG_ATTRIBUTE_KEYWORD(useragent_attribute, "User-agent", std::string)
BOOST_LOG_ATTRIBUTE_KEYWORD(requesttime_attribute, "RequestTime", std::string)
BOOST_LOG_ATTRIBUTE_KEYWORD(pipe_attribute, "Pipe", std::string)
BOOST_LOG_ATTRIBUTE_KEYWORD(requestlength_attribute, "RequestLength", size_t)
BOOST_LOG_ATTRIBUTE_KEYWORD(microserver_attribute, "Microserver", std::string)
BOOST_LOG_ATTRIBUTE_KEYWORD(host_attribute, "Host", std::string)

// ----------------------------------------------------

struct filelogger_base
{
	using sink_backend_t = boost::log::sinks::text_file_backend;
	using sink_frontend_t = boost::log::sinks::synchronous_sink<sink_backend_t>;

	boost::shared_ptr<sink_frontend_t> log_frtnd_;

	filelogger_base(const std::string& log_dir, const std::string& file_prefix, const std::string& channel)
		: log_frtnd_{boost::make_shared<sink_frontend_t>(
						 boost::make_shared<sink_backend_t>(
							boost::log::keywords::file_name = build_path(log_dir, file_prefix) + ".log",
							boost::log::keywords::rotation_size = 1 * 1024 * 1024,
							boost::log::keywords::open_mode = std::ios_base::app
			))}
	{
		using namespace boost::log;

		// minimum scope to make release the locked_ptr
		{
			auto log_bcknd = log_frtnd_->locked_backend();
			// manage log file collection and avoid overwriting existing log files at startup
			log_bcknd->set_file_collector(sinks::file::make_collector(
				keywords::target = build_path(log_dir, file_prefix) + "_old",
				keywords::max_size = 200 * 1024 * 1024, // max total size of collected log files
				keywords::min_free_space = 1024 * 1024 * 1024));
			log_bcknd->scan_for_files();
			log_bcknd->auto_flush(true);
		}

		// set up filtering for local / remote logging
		log_frtnd_->set_filter(expressions::attr<std::string>("Channel") == channel);

		// register the sink to the core
		core::get()->add_sink(log_frtnd_);
	}

	~filelogger_base() noexcept
	{
		// detach sinks from the core, otherwise they will go on logging...
		boost::log::core::get()->remove_sink(log_frtnd_);
		
		/* ensure that sinks are flushed (couldn't find in the
		 * documentation that sinks are flushed at destruction time) */
		log_frtnd_->flush();
	}

	void flush()
	{
		log_frtnd_->flush();
	}

	static std::string build_path(const std::string& log_dir, const std::string& file_name)
	{
		std::string ret = file_name.empty() ? std::string("log") : file_name;
		if(log_dir.empty()) return ret;

		if(log_dir.back() == '/')
			return log_dir + (ret.front() == '/' ? ret.substr(1) : ret);
		else
			return log_dir + (ret.front() == '/' ? ret : ret.insert(0, "/"));
	}
};

// -----------------------------------------------------------------------------------
// access log class implementation
// -----------------------------------------------------------------------------------

class access_log::impl : public filelogger_base
{
public:
	impl(const std::string& log_dir, const std::string& file_prefix);
	impl(const impl&) = delete;
	impl(impl&& l) = default;
	impl& operator=(const impl&) = delete;
	impl& operator=(impl&& l) = default;

	void log(const access_record& r);

private:
	static const std::string channel_name;
};

access_log::impl::impl(const std::string& log_dir, const std::string& file_prefix)
	: filelogger_base{log_dir, file_prefix, channel_name}
{
	using namespace boost::log;

	// set up log formatting
	log_frtnd_->set_formatter
	(
		// 127.0.0.1 - frank [10/Oct/2000:13:55:36 -0700] "GET /apache_pb.gif HTTP/1.0" 200 2326
		// Access log attributes: "%h %l %u %t \"%r\" %>s %b" common
		expressions::format(R"out(%1% %2% %3% [%4%] "%5%" %6% %7% "%8%" "%9%" %10% %11% %12% "%13%" "%14%")out")
			% hostname_attribute
			% logname_attribute
			% user_attribute
			% timestamp_attribute
			% req_attribute
			% status_attribute
			% size_attribute
			% referrer_attribute
			% useragent_attribute
			% requesttime_attribute
			% pipe_attribute
			% requestlength_attribute
			% microserver_attribute
			% host_attribute
	);
}

void access_log::impl::log(const access_record& r)
{
	static thread_local boost::log::sources::channel_logger<std::string> logger_{boost::log::keywords::channel = channel_name};
	static thread_local boost::log::attributes::mutable_constant<std::string> hostname_attr{""};
	static thread_local boost::log::attributes::mutable_constant<std::string> logname_attr{""};
	static thread_local boost::log::attributes::mutable_constant<std::string> user_attr{""};
	static thread_local boost::log::attributes::mutable_constant<std::string> req_attr{""};
	static thread_local boost::log::attributes::mutable_constant<int> status_attr{0};
	static thread_local boost::log::attributes::mutable_constant<size_t> size_attr{0};
	static thread_local boost::log::attributes::mutable_constant<std::string> referrer_attribute{""};
	static thread_local boost::log::attributes::mutable_constant<std::string> useragent_attribute{""};
	static thread_local boost::log::attributes::mutable_constant<std::string> requesttime_attribute{""};
	static thread_local boost::log::attributes::mutable_constant<std::string> pipe_attribute{""};
	static thread_local boost::log::attributes::mutable_constant<size_t> requestlength_attribute{0};
	static thread_local boost::log::attributes::mutable_constant<std::string> microserver_attribute{""};
	static thread_local boost::log::attributes::mutable_constant<std::string> host_attribute{""};

	static thread_local bool init = true;
	if(init)
	{
		init = false;
		logger_.add_attribute("HostName", hostname_attr);
		logger_.add_attribute("LogName", logname_attr);
		logger_.add_attribute("User", user_attr);
		logger_.add_attribute("Req1stLine", req_attr);
		logger_.add_attribute("Status", status_attr);
		logger_.add_attribute("ResponseSize", size_attr);
		logger_.add_attribute("TimeStamp", boost::log::attributes::make_function([]{
			return format::apache_access_time(std::chrono::system_clock::now());
		}));
		logger_.add_attribute("Referrer", referrer_attribute);
		logger_.add_attribute("User-agent", useragent_attribute);
		logger_.add_attribute("RequestTime", requesttime_attribute);
		logger_.add_attribute("Pipe", pipe_attribute);
		logger_.add_attribute("RequestLength", requestlength_attribute);
		logger_.add_attribute("Microserver", microserver_attribute);
		logger_.add_attribute("Host", host_attribute);
	}

	// duration _zero_ must be printed as "0.000"... we must do it by ourselves! thanks boost::log...
	static thread_local std::string req_time_str(16, 'x');
	{
		using namespace std::chrono;
		auto dur = duration_cast<std::chrono::duration<double,std::ratio<1,1>>>(r.end - r.start).count();
		size_t n = snprintf(const_cast<char*>(req_time_str.data()), req_time_str.capacity(), "%.3f", dur);
		req_time_str.resize(n);
		if(n >= req_time_str.capacity())
			req_time_str.assign(req_time_str.size(), 'x');
	}

	hostname_attr.set(r.remote_addr.empty() ? hypen : static_cast<std::string>( r.remote_addr ) );
	logname_attr.set(hypen);
	user_attr.set(r.remote_user.empty() ? hypen : static_cast<std::string>( r.remote_user) );
	req_attr.set( static_cast<std::string>( r.req ) );
	status_attr.set( r.status );
	size_attr.set(r.body_bytes_sent);
	referrer_attribute.set(r.referer.empty() ? hypen : static_cast<std::string>( r.referer ) );
	useragent_attribute.set(r.user_agent.empty() ? hypen : static_cast<std::string>( r.user_agent) );
	requesttime_attribute.set(req_time_str);
	pipe_attribute.set(r.pipe ? p : dot);
	requestlength_attribute.set(r.req_length);
	microserver_attribute.set(r.x_debug_cyn_ip.empty() ? hypen : static_cast<std::string>( r.x_debug_cyn_ip) ) ;
	host_attribute.set(r.host.empty() ? hypen : static_cast<std::string>( r.host ) );

	BOOST_LOG(logger_);
}

std::ostream& operator<<(std::ostream& os, severity level)
{
	switch(level)
	{
	case severity::emerg:
		os << "emerg";
		break;
	case severity::alert:
		os << "alert";
		break;
	case severity::crit:
		os << "crit";
		break;
	case severity::error:
		os << "error";
		break;
	case severity::warn:
		os << "warn";
		break;
	case severity::notice:
		os << "notice";
		break;
	case severity::info:
		os << "info";
		break;
	case severity::debug:
		os << "debug";
		break;
	case severity::trace1:
		os << "trace1";
		break;
	case severity::trace2:
		os << "trace2";
		break;
	case severity::trace3:
		os << "trace3";
		break;
	case severity::trace4:
		os << "trace4";
		break;
	case severity::trace5:
		os << "trace5";
		break;
	case severity::trace6:
		os << "trace6";
		break;
	case severity::trace7:
		os << "trace7";
		break;
	case severity::trace8:
		os << "trace8";
		break;
	}

	return os;
}

// ----- static vars initialization
const std::string access_log::impl::channel_name = "access";

// *************************** access log ********************************
access_log::access_log(const std::string& log_dir, const std::string& file_prefix)
	: pImpl{new impl{log_dir, file_prefix}}
{}

void access_log::log(const access_record& r) noexcept
{
	pImpl->log(r);
}

void access_log::flush()
{
	pImpl->flush();
}

access_log::~access_log() {}

}

#ifndef DOORMAT_LOG_FORMAT_H_
#define DOORMAT_LOG_FORMAT_H_

#include <chrono>
#include <string>


namespace logging
{
namespace format
{

/**
 * @brief apache_access_time converts a system clock's time point to
 * an Apache-like access time string.
 * @param t system clock time point
 * @return Apache access log-like formatted timestamp
 */
std::string apache_access_time(std::chrono::system_clock::time_point t);


/**
 * @brief apache_error_time converts a system clock's time point to
 * an Apache-like error time string.
 * @param t system clock time point
 * @return Apache error log-like formatted timestamp
 */
std::string apache_error_time(std::chrono::system_clock::time_point t);

/**
 * @brief http_header_date converts a system clock's time point to
 * an http date header-like string.
 * @param t system clock time point
 * @return formatted timestamp like http date's header
 */
std::string http_header_date(std::chrono::system_clock::time_point t);

}
}




#endif // DOORMAT_LOG_FORMAT_H_

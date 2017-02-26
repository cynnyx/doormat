#ifndef DOORMAT_LOG_H_
#define DOORMAT_LOG_H_

#include <cstdint>
#include <string>
#include <memory>
#include <ostream>
#include <chrono>

#include "access_record.h"

namespace logging
{

enum class severity : uint8_t
{
	trace8,
	trace7,
	trace6,
	trace5,
	trace4,
	trace3,
	trace2,
	trace1,
	debug,
	info,
	notice,
	warn,
	error,
	crit,
	alert,
	emerg
};

std::ostream& operator<<(std::ostream& os, severity level);

class access_log final
{
	class impl;
	std::unique_ptr<impl> pImpl;

public:
	using duration = std::chrono::milliseconds;

	access_log(const std::string& log_dir, const std::string& file_prefix);
	access_log(const access_log&) = delete;
	access_log(access_log&& l) = delete;
	access_log& operator=(const access_log&) = delete;
	access_log& operator=(access_log&& l) = delete;
	~access_log();

	void log(const access_record& r) noexcept;
	void flush();
};

}

#endif // DOORMAT_LOG_H_

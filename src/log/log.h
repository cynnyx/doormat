#ifndef DOORMAT_LOG_H_
#define DOORMAT_LOG_H_

#include <string>
#include <memory>

namespace logging
{

struct access_record;

class access_log
{
public:
	access_log() = default;
	access_log(const access_log&) = delete;
	access_log(access_log&& l) = delete;
	access_log& operator=(const access_log&) = delete;
	access_log& operator=(access_log&& l) = delete;
	virtual ~access_log() = default;
	virtual void log(const access_record& r) noexcept = 0;
	virtual void flush() = 0;
};

// Obsolete implementation with boost::log 
/// @todo move to spdlog
class access_log_c: public access_log
{
	class impl;
	std::unique_ptr<impl> pImpl;
public:
	access_log_c(const std::string& log_dir, const std::string& file_prefix);
	~access_log_c();

	void log(const access_record& r) noexcept override;
	void flush() override;
};

class dummy_al : public access_log
{
public:
	void log(const access_record& r) noexcept override { /* empty */}
	void flush() override { /* empty */}
};

}

#endif // DOORMAT_LOG_H_

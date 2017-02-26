#ifndef DOORMAT_INSPECTOR_SERIALIZER_H
#define DOORMAT_INSPECTOR_SERIALIZER_H

#include <fstream>
#include <list>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "../http/http_structured_data.h"
#include "../utils/json.hpp"
#include "access_record.h"

namespace logging
{

class writer
{
public:
	virtual void open( const std::string& ) = 0;
	virtual writer& operator<< ( const std::string& ) = 0;
	virtual ~writer() = default;
};

/**
 * @note This class is not thread safe
 */
class inspector_log final
{
	std::unique_ptr<writer> logfile;

	void jsonize_headers( nlohmann::json& json, const http::http_structured_data::headers_map& hrs );
	void jsonize_body( nlohmann::json& json, const std::list<dstring>& body );
	void jsonize(  access_recorder& r ) noexcept;
public:
	inspector_log( const std::string &log_dir, const std::string &file_prefix, bool active, writer *w = nullptr );
	inspector_log( const inspector_log& ) = delete;
	inspector_log& operator=( const inspector_log& ) = delete;
	~inspector_log();

	void log( access_recorder&& r ) noexcept;
	bool active() noexcept;
};

}
#endif //DOORMAT_INSPECTOR_SERIALIZER_H

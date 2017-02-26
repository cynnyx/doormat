#ifndef CONFIGURATIONEXCEPTION_H
#define CONFIGURATIONEXCEPTION_H

#include <stdexcept>
#include <boost/optional.hpp>

namespace configuration 
{
	
class configuration_exception : public std::runtime_error
{
	boost::optional<std::exception> ex;
public:
	configuration_exception(const std::string& msg): std::runtime_error(msg) {}
	configuration_exception(const std::exception& ex_, const std::string& msg):
		std::runtime_error(msg), ex(ex_) {}
	boost::optional<std::exception> getOriginalException() const
	{
		return ex;
	}
};

}

#endif // CONFIGURATIONEXCEPTION_H

#include <stdexcept>
#include <string>

#include <nghttp2/nghttp2.h>

namespace http2
{
namespace errors
{
	
class failure : virtual public std::exception
{
	int nge{0};
	std::string error;
	const char* filename;
	int line;
public:
	failure( const std::string& e, const char* f, int l ): error(e), filename(f), line(l) {}
	failure( int n, const char* f, int l ): nge(n), filename(f), line(l) {}
	virtual const char* what() const noexcept { if ( nge == 0 ) return error.c_str(); else return nghttp2_strerror( nge ); }
	virtual std::string what_formatted() const 
	{ 
		if ( nge == 0 ) 
			return error + " filename: " + filename + " line: " + std::string{ utils::from<int>( line ) }; 
		else
			return std::string{ nghttp2_http2_strerror( nge ) } + " filename: " + filename 
				+ " line: " + std::string{ utils::from<int>( line ) };
	}
	virtual ~failure() noexcept = default;
};
	
class setting_connection_failure : virtual public failure 
{
public:
	setting_connection_failure( const std::string& e, const char* f, int l ): failure(e,f,l) {}
	setting_connection_failure( int n, const char* f, int l ): failure( n,f,l) {}
};

class session_recv_failure : virtual public failure 
{
public:
	session_recv_failure( const std::string& e, const char* f, int l ): failure(e,f,l) {}
	session_recv_failure( int n, const char* f, int l ): failure( n,f,l) {}
};

class session_send_failure : virtual public failure 
{
public:
	session_send_failure( const std::string& e, const char* f, int l ): failure(e,f,l) {}
	session_send_failure( int n, const char* f, int l ): failure( n,f,l) {}
};

class too_many_streams_failure : virtual public failure 
{
public:
	too_many_streams_failure( const std::string& e, const char* f, int l ): failure(e,f,l) {}
	too_many_streams_failure( int n, const char* f, int l ): failure( n,f,l) {}
};

class failing_at_failing : virtual public failure
{
public:
	failing_at_failing( const std::string& e, const char* f, int l ): failure(e,f,l) {}
	failing_at_failing( int n, const char* f, int l ): failure( n,f,l) {}
};

}
}

// Warning - macros jumps in and out of namespaces
// exception is the type of the exception
// error is a string or an int returned by nghttp2!
#define THROW(exception, error) { throw exception(error, __FILE__, __LINE__  ); }

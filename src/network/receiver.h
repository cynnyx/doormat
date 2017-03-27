#ifndef RECEIVER___H
#define RECEIVER___H

#include "../utils/dstring.h"
#include "../http/http_response.h"

namespace network
{
	
/**
 * @class receiver
 * 
 * This interface is to be implemented for those who want to receive connector's events.
 */
class receiver
{
public:
	virtual ~receiver() = default;
	virtual void on_header(http::http_response &&) noexcept = 0;
	virtual void on_body(dstring&&) noexcept = 0;
	virtual void on_trailer(dstring&&, dstring&&) noexcept = 0;
	virtual void on_eom() noexcept = 0;
	virtual void on_error( int error ) noexcept = 0;
	virtual void stop() noexcept = 0;
};

}
#endif

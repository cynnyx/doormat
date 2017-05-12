#include "../configuration/configuration_wrapper.h"
#include "../dummy_node.h"
#include "../http2/session.h"
#include "handler_http1.h"
#include "handler_factory.h"
#include "http_handler.h"

namespace server {

boost::asio::ip::address http_handler::find_origin() const
{

	if(auto s = _connector.lock()) return s->origin();
	return {};
}

void http_handler::close()
{
	if(auto s = _connector.lock()) s->close();
}

void http_handler::connector(std::shared_ptr<server::connector_interface>  conn )
{
	LOGTRACE("http_handler::connector ", conn );
	_connector = conn;
	if ( ! _connector.lock() )
		on_connector_nulled();
}

void http_handler::set_timeout(std::chrono::milliseconds ms)
{
	if(auto s = _connector.lock())
	{
		s->set_timeout(std::move(ms));
	}
}

}
#include "handler_factory.h"
#include "http_handler.h"

namespace server 
{
	
std::shared_ptr<connector_interface> http_handler::connector()
{
	return _connector.lock();
}

std::shared_ptr<const connector_interface> http_handler::connector() const
{
	return _connector.lock();
}

boost::asio::ip::address http_handler::find_origin() const
{
	if(auto s = _connector.lock()) return s->origin();
	return {};
}

void http_handler::connector(std::shared_ptr<server::connector_interface>  conn )
{
	LOGTRACE("http_handler::connector ", conn );
	_connector = conn;
	if ( ! _connector.lock() )
		on_connector_nulled();
}

}

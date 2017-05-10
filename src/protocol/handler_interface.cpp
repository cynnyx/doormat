#include "../configuration/configuration_wrapper.h"
#include "../dummy_node.h"
#include "../http2/session.h"
#include "handler_http1.h"
#include "handler_factory.h"
#include "handler_interface.h"

namespace server {

boost::asio::ip::address handler_interface::find_origin() const
{
	if(connector()) return connector()->origin();
	return {};
}

void handler_interface::close() {
	if(auto s = _connector.lock()) s->close();
}

void handler_interface::connector(std::shared_ptr<server::connector_interface>  conn )
{
	LOGTRACE("handler_interface::connector ", conn );
	_connector = conn;
	if ( ! _connector.lock() )
		on_connector_nulled();
}


}
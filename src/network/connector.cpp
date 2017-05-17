#include "connector.h"

#include "connector_clear.h"
#include "connector_ssl.h"

// TODO: kill me?

namespace network
{

std::unique_ptr<connector> create_connector( const http::http_request &req, receiver& rec )
{
	if ( req.ssl() )
		return std::unique_ptr<connector_ssl>{ new connector_ssl{ req, rec } };
	else
		return std::unique_ptr<connector_clear>{ new connector_clear{ req, rec } };
}

}

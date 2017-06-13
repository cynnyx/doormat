#include "http_client.h"

#include "protocol/handler_http1.h"
#include "http2/session_client.h"
#include "http/client/client_connection.h"
#include "http/client/response.h"

namespace client
{
namespace detail
{
//using connect_callback_t = cra
using connector_ptr = std::shared_ptr<server::connector_interface>;


handler_from_connector_factory::handler_from_connector_factory(connect_callback_t cb) noexcept
	: cb_{std::move(cb)}
{}

void handler_from_connector_factory::operator()(connector_ptr c, http::proto_version v)
{
	if(v == http::proto_version::HTTP20)
	{
		auto h = std::make_shared<http2::session_client>();
		c->handler(h);
		c->start(true);
		cb_(std::move(h));
	}
	else
	{
		auto h = std::make_shared<server::handler_http1<http::client_traits>>();
		h->connector(c);
		c->handler(h);
		c->start(true);
		cb_(std::move(h));

	}
}

}
}

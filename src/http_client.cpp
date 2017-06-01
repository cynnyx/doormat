#include "http_client.h"



namespace client
{
namespace detail
{
//using connect_callback_t = cra
using connector_ptr = std::shared_ptr<server::connector_interface>;


handler_from_connector_factory::handler_from_connector_factory(http::proto_version v, connect_callback_t cb) noexcept
	: v_{v}
	, cb_{std::move(cb)}
{}

void handler_from_connector_factory::operator()(connector_ptr c)
{
	if(v_ == http::proto_version::HTTP20)
	{
		auto h = std::make_shared<http2::session>();
		c->handler(h);
		c->start(true);
//		cb_(std::move(h));
		assert(0 && "We don't support http2 afan");
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

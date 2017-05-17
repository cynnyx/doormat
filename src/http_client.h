#ifndef DOORMAT_HTTP_CLIENT_H
#define DOORMAT_HTTP_CLIENT_H

#include <functional>
#include <memory>

#include "http/http_commons.h"
#include "http/client/client_traits.h"
#include "protocol/handler_http1.h"

namespace http {
class client_connection;
}

// TODO: hmm... namespace client sucks... both client and server should be inside the http namespace
// and maybe define their own namespace
namespace client
{


template<typename connector_factory_t>
class http_client
{
	connector_factory_t connector_factory_;

public:
	using connect_callback = std::function<void(std::shared_ptr<http::client_connection>)>;
	using error_callback = std::function<void(int)>; // TODO: int? come on... :D

	template<typename... Args>
	http_client(Args&&... args)
		: connector_factory_{std::forward<Args>(args)...}
	{}

	template<typename... Args>
	void connect(connect_callback ccb, error_callback ecb, bool tls, Args&&... args)
	{
		connector_factory_(std::forward<Args>(args)..., tls, [ccb](auto connector_ptr)
		{
			auto h = std::make_shared<server::handler_http1<http::client_traits>>(http::proto_version::HTTP11); // TODO: it's only http1 for now
			h->connector(connector_ptr);
			connector_ptr->handler(h);
			connector_ptr->start(true);
			ccb(std::move(h));
		},
		[ecb](const auto& error)
		{
			ecb(error);
		});
	}
};


} // namespace client

#endif // DOORMAT_HTTP_CLIENT_H

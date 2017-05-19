#ifndef DOORMAT_HTTP_CLIENT_H
#define DOORMAT_HTTP_CLIENT_H

#include <functional>
#include <memory>
#include <utility>

#include "http/http_commons.h"
#include "http/client/client_traits.h"

namespace http {
class client_connection;
}
namespace server
{
template<typename>
class handler_http1;
class connector_interface;
}

// TODO: hmm... namespace client sucks... both client and server should be inside the http namespace
// and maybe define their own namespace
namespace client
{

namespace detail
{
struct handler_factory
{
	using connect_callback_t = std::function<void(std::shared_ptr<http::client_connection>)>;

	handler_factory(http::proto_version v, connect_callback_t cb) noexcept;
	void operator()(std::shared_ptr<server::connector_interface>);
private:
	http::proto_version v_;
	connect_callback_t cb_;
};

}

template<typename connector_factory_t>
class http_client
{
	connector_factory_t connector_factory_;

public:
	using connect_callback_t = detail::handler_factory::connect_callback_t;
	using error_callback_t = std::function<void(int)>; // TODO: int? come on... :D

	template<typename... Args>
	http_client(Args&&... args)
		: connector_factory_{std::forward<Args>(args)...}
	{}

	template<typename... connector_args_t>
	void connect(connect_callback_t ccb, error_callback_t ecb, http::proto_version v, bool tls, connector_args_t&&... args)
	{
		connector_factory_.get_connector(std::forward<connector_args_t>(args)..., tls, detail::handler_factory(v, std::move(ccb)),
		[ecb](const auto& error)
		{
			ecb(error);
		});
	}
};


} // namespace client

#endif // DOORMAT_HTTP_CLIENT_H

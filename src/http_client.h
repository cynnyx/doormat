#ifndef DOORMAT_HTTP_CLIENT_H
#define DOORMAT_HTTP_CLIENT_H

#include <functional>
#include <memory>
#include <utility>

#include "http/http_commons.h"
#include "http/client/client_traits.h"
#include "network/communicator/communicator_factory.h"
#include "protocol/handler_http1.h"
#include "http2/stream.h"
#include "http/client/client_connection.h"
#include "http/client/response.h"

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
struct handler_from_connector_factory
{
	using connect_callback_t = std::function<void(std::shared_ptr<http::client_connection>)>;

	handler_from_connector_factory(connect_callback_t cb) noexcept;
	void operator()(std::shared_ptr<server::connector_interface>, http::proto_version);
private:
	connect_callback_t cb_;
};

template<typename connector_factory_t>
class handler_factory
{

	using error_callback_t = std::function<void(int)>; //fixme;
	connector_factory_t factory;

public:
	using connect_callback_t = std::function<void(std::shared_ptr<http::client_connection>)>;
	template<typename... construction_args_t>
	handler_factory(construction_args_t&&...args) : factory{std::forward<construction_args_t>(args)...}
	{}

	template<typename... connector_args_t>
	void get_connection(connect_callback_t ccb, error_callback_t ecb, http::proto_version v, connector_args_t&&... args)
	{
		factory.get_connector(std::forward<connector_args_t>(args)..., [v, ccb = std::move(ccb)](auto conn, auto){
			if(v == http::proto_version::HTTP20)
			{
				assert(false && "currently http20 is not supported on the client side!");
			} else
			{
				auto h = std::make_shared<server::handler_http1<http::client_traits>>();
				h->connector(conn);
				conn->handler(h);
				conn->start(true);
				ccb(std::move(h));
			}
		}, ecb);
	}

};


}

template<typename factory, typename Enable = void>
class http_client;

/** Specialization for connector factory*/
template<typename connector_factory_t>
class http_client<connector_factory_t, std::enable_if_t<std::is_base_of<network::connector_factory, connector_factory_t>::value>>
{
	connector_factory_t connector_factory_;

public:
	using connect_callback_t = detail::handler_from_connector_factory::connect_callback_t;
	using error_callback_t = std::function<void(int)>; // TODO: int? come on... :D

	template<typename... Args>
	http_client(Args&&... args)
	: connector_factory_{std::forward<Args>(args)...}
	{}

	template<typename... connector_args_t>
	void connect(connect_callback_t ccb, error_callback_t ecb, http::proto_version v, connector_args_t&&... args)
	{
		connector_factory_.get_connector(std::forward<connector_args_t>(args)..., detail::handler_from_connector_factory(std::move(ccb)),
		                                 [ecb](const auto& error)
		                                 {
			                                 ecb(error);
		                                 });
	}
};

/** Specialization for handler factory!*/
template<typename handler_factory_t>
class http_client<handler_factory_t, std::enable_if_t<!std::is_base_of<network::connector_factory, handler_factory_t>::value>>
{
	handler_factory_t  factory;
public:
	using connect_callback_t = typename handler_factory_t::connect_callback_t;
	using error_callback_t = std::function<void(int)>; //fixme

	template<typename... Args>
	http_client(Args&&... args) : factory{std::forward<Args>(args)...}
	{}

	template<typename... handler_args_t>
	void connect(connect_callback_t ccb, error_callback_t ecb, handler_args_t&&... args)
	{
		factory.get_connection(ccb, ecb, std::forward<handler_args_t>(args)...);
	}
};


} // namespace client

#endif // DOORMAT_HTTP_CLIENT_H

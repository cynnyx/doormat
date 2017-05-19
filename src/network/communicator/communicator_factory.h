#ifndef DOORMAT_CONNECTOR_FACTORY_H
#define DOORMAT_CONNECTOR_FACTORY_H

#include <memory>
#include <cstdint>
#include <string>

namespace http {
class http_request;
}

namespace server { // TODO: well, actually...
class connector_interface;
}

namespace network {


class connector_factory
{
public:
	using connector_callback_t = std::function<void(std::shared_ptr<server::connector_interface>)>;
	using error_callback_t = std::function<void(int)>;
	virtual void get_connector(const std::string& address, uint16_t port, bool tls, connector_callback_t, error_callback_t)=0;
	virtual void stop()=0;
	virtual ~connector_factory()=default;
};

}


#endif //DOORMAT_CONNECTOR_FACTORY_H

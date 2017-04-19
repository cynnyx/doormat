#ifndef DOORMAT_CONNECTOR_FACTORY_H
#define DOORMAT_CONNECTOR_FACTORY_H
#include <memory>
#include "communicator.h"

namespace http {
    class http_request;
}

namespace network {

class connector_interface;

class communicator_factory
{
public:
    using connector_callback_t = std::function<void(std::unique_ptr<communicator_interface>)>;
    using error_callback_t = std::function<void(int)>;
    virtual void get_connector(const http::http_request &, connector_callback_t, error_callback_t)=0;
    virtual void stop()=0;
    virtual ~communicator_factory()=default;
};

}


#endif //DOORMAT_CONNECTOR_FACTORY_H

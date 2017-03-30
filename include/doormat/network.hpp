#ifndef DOORMAT_NETWORK_HPP_
#define DOORMAT_NETWORK_HPP_

#include "../../src/network/cloudia_pool.h"
#include "../../src/network/socket_factory.h"

namespace doormat {

using ::network::cloudia_pool;
using ::network::cloudia_pool_factory;
template<typename S>
using abstract_factory_of_socket_factory = ::network::abstract_factory_of_socket_factory<S>;

}

#endif // DOORMAT_NETWORK_HPP_

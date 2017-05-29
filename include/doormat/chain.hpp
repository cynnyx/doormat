#ifndef DOORMAT_CHAIN_HPP_
#define DOORMAT_CHAIN_HPP_

#include "../../src/chain_of_responsibility/chain_of_responsibility.h"
#include "../../src/endpoints/chain_factory.h"

#include <memory>

namespace doormat {

using ::endpoints::chain_factory;
template<typename T, typename S, typename... N>
using chain = ::chain<T, S, N...>;

template<typename T,typename ...Args>
std::shared_ptr<T> make_shared_chain(logging::access_recorder *aclogger = nullptr)
{
	return ::make_shared_chain<T, Args...>(aclogger);
}

}

#endif // DOORMAT_CHAIN_HPP_

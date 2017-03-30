#ifndef DOORMAT_CHAIN_HPP_
#define DOORMAT_CHAIN_HPP_

#include "../../src/chain_of_responsibility/chain_of_responsibility.h"
#include "../../src/endpoints/chain_factory.h"

namespace doormat {

using ::endpoints::chain_factory;
template<typename T, typename S, typename... N>
using chain = ::chain<T, S, N...>;

template<typename T,typename ...Args>
std::unique_ptr<T> make_unique_chain(logging::access_recorder *aclogger = nullptr)
{
    return ::make_unique_chain<T, Args...>(aclogger);
}

}

#endif // DOORMAT_CHAIN_HPP_

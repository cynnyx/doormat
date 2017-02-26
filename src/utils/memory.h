#ifndef DOORMAT_MEMORY_H_H
#define DOORMAT_MEMORY_H_H

namespace utils
{

template<typename T>
constexpr std::size_t calculate_space_of_T()
{
	return ( ( sizeof(T) - 1 ) / alignof(T) + 1) * alignof(T);
}

}
#endif //DOORMAT_MEMORY_H_H

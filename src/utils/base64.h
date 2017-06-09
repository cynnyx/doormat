#ifndef DOORMAT_BASE_64_H_
#define DOORMAT_BASE_64_H_

#include <string>

namespace utils
{

std::string base64_encode(unsigned char const* , unsigned int len);
std::string base64_decode(std::string const& s);

}

#endif

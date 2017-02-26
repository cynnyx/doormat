#ifndef DOORMAT_BASE_64_H_
#define DOORMAT_BASE_64_H_

#include "dstring.h"

namespace utils
{

dstring base64_encode(unsigned char const* , unsigned int len);
dstring base64_decode(std::string const& s);

}


#endif

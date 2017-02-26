#ifndef DOORMAT_UTILS_H_H
#define DOORMAT_UTILS_H_H


#include <boost/regex.hpp>

namespace cache_req_elaborations
{
	const static boost::regex nocache{"no-cache|no-store"};

	const static boost::regex maxage{"[s-]?max[-]?age\\s?=\\s?(\\d+)"};
}


#endif //DOORMAT_UTILS_H_H

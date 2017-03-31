#ifndef DOORMAT_HTTP_HPP_
#define DOORMAT_HTTP_HPP_

#include "../../src/http/http_request.h"
#include "../../src/http/http_response.h"
#include "../../src/http/http_commons.h"

#include <utility>

namespace doormat {

using http::http_request;
using http::http_response;
using http::proto_version;
using http::tranfer_encoding;
using http::content_type_t;
using http::proto_version;
using http::proto_version;


inline
const char* content_type(content_type_t type) {
    return ::http::content_type(type);
}

inline
content_type_t content_type( const std::string& filename ) {
    return http::content_type(std::move(filename));
}

}

#endif // DOORMAT_HTTP_HPP_

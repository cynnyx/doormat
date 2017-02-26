#ifndef DOORMAT_TESTCOMMON_H_
#define DOORMAT_TESTCOMMON_H_

#include "../src/http/http_structured_data.h"
#include "../src/http/http_commons.h"
#include "../src/http/http_request.h"

#include <string>


// build request preamble with headers in chromium style#include "../src/http/http_request.h"
http::http_request make_request(http::proto_version protocol, http_method method, const dstring& host, const dstring& path);

// html pages
extern const std::string forbidden_html;
extern const std::string method_not_allowed_html;
extern const std::string request_timeout_html;
extern const std::string internal_server_error_html;
extern const std::string bad_gateway_html;
extern const std::string service_unavailable_html;
extern const std::string gateway_timeout_html;

std::string request_generator(const http_method method, const std::string& content, bool chunked = false );



#endif // DOORMAT_TESTCOMMON_H_

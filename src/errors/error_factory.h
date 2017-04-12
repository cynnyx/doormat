#ifndef CLOUDIA_SIMPLE_ERROR_FACTORY_H
#define CLOUDIA_SIMPLE_ERROR_FACTORY_H

#include <functional>
#include "../http/http_response.h"

#include "error_codes.h"

namespace errors {


class error_factory
{

    using header_callback_t = std::function<void(http::http_response)>;
    using body_callback_t = std::function<void(dstring)>;
    using trailer_callback_t = std::function<void(dstring, dstring)>;
    using eom_callback_t = std::function<void()>;

    http_error_code ec;
    http::proto_version pv;
public:
    error_factory(http_error_code errc, http::proto_version proto_version) : ec{errc}, pv{proto_version}
    {}


    void produce_error_response(header_callback_t hcb, body_callback_t bcb, trailer_callback_t tcb, eom_callback_t ecb) {
        http::http_response res;
        std::string errmsg = errors::to_string(ec);
        res.status(static_cast<uint16_t>(ec), dstring{errmsg.data(), errmsg.size()});
        res.protocol(pv);
        res.content_len(errmsg.size()); 
        hcb(std::move(res));
        bcb(dstring{errmsg.data(), errmsg.size()});
        ecb();
        return;
    }

    ~error_factory() = default;

};

}


#endif //CLOUDIA_SIMPLE_ERROR_FACTORY_H

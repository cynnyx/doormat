#include "chain_factory.h"
#include <memory>

namespace endpoints {

#define GET_BUCKET 0
#define POST_BUCKET 1
#define PUT_BUCKET 2


void chain_factory::get(const std::string &path, generating_function_t logic)
{
    if(!method_prefixes[GET_BUCKET]) {
        method_prefixes[GET_BUCKET] = std::make_unique<radix_tree>("");
    }
    method_prefixes[GET_BUCKET]->addPattern(path, std::move(logic));
}


void chain_factory::post(const std::string &path, generating_function_t logic)
{
    if(!method_prefixes[POST_BUCKET]) {
        method_prefixes[POST_BUCKET] = std::make_unique<radix_tree>("");
    }
    method_prefixes[POST_BUCKET]->addPattern(path, std::move(logic));
}

void chain_factory::put(const std::string &path, generating_function_t logic)
{
    if(!method_prefixes[PUT_BUCKET]) {
        method_prefixes[PUT_BUCKET] = std::make_unique<radix_tree>("");
    }
    method_prefixes[PUT_BUCKET]->addPattern(path, std::move(logic));
}

std::unique_ptr<node_interface> chain_factory::get_chain(const http::http_request &original_request) const
{
    int bucket;
    switch (original_request.method_code()) {
        case http_method::HTTP_GET: bucket = GET_BUCKET; break;
        case http_method::HTTP_POST: bucket = POST_BUCKET; break;
        case http_method::HTTP_PUT: bucket = PUT_BUCKET; break;
        default: return nullptr;
    }
    if(method_prefixes[bucket] == nullptr) return nullptr;
    return method_prefixes[bucket]->get(std::string(original_request.path()));
}


}
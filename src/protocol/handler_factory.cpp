#include "handler_factory.h"
#include "handler_http1.h"
#include "../http2/session.h"
#include "../connector.h"
#include "../utils/log_wrapper.h"
#include "../chain_of_responsibility/node_interface.h"
#include "../chain_of_responsibility/error_code.h"
#include "../chain_of_responsibility/chain_of_responsibility.h"
#include "../dummy_node.h"
#include "../utils/likely.h"
#include "../configuration/configuration_wrapper.h"

#include <string>
#include "../connector.h"

using namespace std;

namespace server
{

const static string alpn_protos_default[]
{
	"\x2h2",
	"\x5h2-16",
	"\x5h2-14",
	"\x8http/1.1",
	"\x8http/1.0"
};
const static string alpn_protos_legacy[]
{\
	"\x8http/1.1",
	"\x8http/1.0"
};

const static unsigned char npn_protos[]
{
	2,'h','2',
	5,'h','2','-','1','6',
	5,'h','2','-','1','4',
	8,'h','t','t','p','/','1','.','1',
	8,'h','t','t','p','/','1','.','0'
};
const static unsigned char npn_protos_legacy[]
{
	8,'h','t','t','p','/','1','.','1',
	8,'h','t','t','p','/','1','.','0'
};

using alpn_cb = int (*)
	(ssl_st*, const unsigned char**, unsigned char*, const unsigned char*, unsigned int, void*);

alpn_cb alpn_select_cb = [](ssl_st *ctx, const unsigned char** out, unsigned char* outlen,
		const unsigned char* in, unsigned int inlen, void *data)
{
	//string local_alpn_protos[5] = (service_locator::configuration().http2_is_disabled()) ?  alpn_protos_legacy : alpn_protos_default;
	if(service::locator::configuration().http2_is_disabled())
	{
		for( auto& proto : alpn_protos_legacy )
			if(SSL_select_next_proto((unsigned char**)out, outlen, (const unsigned char*)proto.c_str(), proto.size()-1, in, inlen) == OPENSSL_NPN_NEGOTIATED)
				return SSL_TLSEXT_ERR_OK;
	}
	else
	{
		for( auto& proto : alpn_protos_default )
			if(SSL_select_next_proto((unsigned char**)out, outlen, (const unsigned char*)proto.c_str(), proto.size()-1, in, inlen) == OPENSSL_NPN_NEGOTIATED)
				return SSL_TLSEXT_ERR_OK;
	}
	return SSL_TLSEXT_ERR_NOACK;
};

using npn_cb = int (*) (SSL *ssl, const unsigned char** out, unsigned int* outlen, void *arg);
npn_cb npn_adv_cb = [](ssl_st *ctx, const unsigned char** out, unsigned int* outlen, void *arg)
{
	*out = (service::locator::configuration().http2_is_disabled()) ? npn_protos_legacy : npn_protos;
	*outlen = sizeof(npn_protos);
	return SSL_TLSEXT_ERR_OK;
};

void handler_factory::register_protocol_selection_callbacks(SSL_CTX* ctx)
{
	SSL_CTX_set_next_protos_advertised_cb(ctx, npn_adv_cb, nullptr);
	SSL_CTX_set_alpn_select_cb(ctx, alpn_select_cb, nullptr);
}

std::shared_ptr<handler_interface> handler_factory::negotiate_handler(std::shared_ptr<ssl_socket> sck, interval connect_timeout, interval read_timeout) const noexcept
{
	const unsigned char* proto{nullptr};
	unsigned int len{0};

    const SSL *ssl_handle = sck->native_handle();

	//ALPN
	SSL_get0_alpn_selected(ssl_handle, &proto, &len);
	if( !len )
		//NPN
		SSL_get0_next_proto_negotiated(ssl_handle, &proto, &len);


	handler_type type{ht_h1};
	if( len )
	{
		LOGDEBUG("Protocol negotiated: ", std::string(reinterpret_cast<const char*>(proto), len));
		switch(len)
		{
			case 2:
			case 3:
			case 5:
				type = ht_h2;
				break;
		}
	}

	http::proto_version version = (type == ht_h2) ? http::proto_version::HTTP20 : ((proto == nullptr ||   proto[len-1] == '0') ? http::proto_version::HTTP10 : http::proto_version::HTTP11);


	return build_handler( type , version, connect_timeout, read_timeout, sck);
}

boost::asio::ip::address handler_interface::find_origin() const
{
    if(connector()) return connector()->origin();
    return {};
}

std::shared_ptr<handler_interface> handler_factory::build_handler(handler_type type, http::proto_version proto, interval _connect_timeout, interval _read_timeout, std::shared_ptr<tcp_socket> socket) const noexcept
{
    auto h = make_handler(type, proto);
    auto conn = std::make_shared<connector<tcp_socket>>(_connect_timeout, _read_timeout, socket);
    conn->handler(h);
    conn->start(true);
    return h;
}

std::shared_ptr<handler_interface> handler_factory::build_handler(handler_type type, http::proto_version proto, interval _connect_timeout, interval _read_timeout, std::shared_ptr<ssl_socket> socket) const noexcept
{
    auto conn = std::make_shared<connector<ssl_socket>>(_connect_timeout, _read_timeout, socket);
    auto h = make_handler(type, proto);
    conn->handler( h );
    conn->start(true);
    return h;
}

std::shared_ptr<handler_interface> handler_factory::make_handler(handler_type type, http::proto_version proto ) const noexcept {
    switch(type)
    {
        case ht_h1:
            LOGDEBUG("HTTP1 selected");
            return std::make_shared<handler_http1>( proto );
        case ht_h2:
            LOGTRACE("HTTP2 NG selected");
            return std::make_shared<http2::session>();
        case ht_unknown: return nullptr;
    }
    return nullptr;
}

void handler_interface::initialize_callbacks(node_interface &cor)
{
	header_callback hcb = [this](http::http_response&& headers){ /*on_header(move(headers)); */ };
	body_callback bcb = [this](dstring&& chunk){ /* on_body(std::move(chunk)); */};
	trailer_callback tcb = [this](dstring&& k, dstring&& v){ /* on_trailer(move(k),move(v)); */};
	end_of_message_callback eomcb = [this](){on_eom();};
	error_callback ecb = [this](const errors::error_code& ec) { on_error( ec.code() ); };
	response_continue_callback rccb = [this](){ /*on_response_continue(); */};

    cor.initialize_callbacks(hcb, bcb, tcb, eomcb, ecb, rccb);
}

void handler_interface::connector( std::shared_ptr<connector_interface>  conn )
{
	LOGTRACE("handler_interface::connector ", conn );
	_connector = conn;
	if ( ! _connector.lock() )
		on_connector_nulled();
}
} //namespace

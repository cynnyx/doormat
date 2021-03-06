#include "handler_factory.h"

#include <string>

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

const static unsigned char npn_protos[]
{
	2,'h','2',
	5,'h','2','-','1','6',
	5,'h','2','-','1','4',
	8,'h','t','t','p','/','1','.','1',
	8,'h','t','t','p','/','1','.','0'
};

using alpn_cb = int (*)
(ssl_st*, const unsigned char**, unsigned char*, const unsigned char*, unsigned int, void*);

alpn_cb alpn_select_cb = [](ssl_st *ctx, const unsigned char** out, unsigned char* outlen,
const unsigned char* in, unsigned int inlen, void *data)
{
	for( auto& proto : alpn_protos_default )
		if(SSL_select_next_proto((unsigned char**)out, outlen, (const unsigned char*)proto.c_str(), proto.size()-1, in, inlen) == OPENSSL_NPN_NEGOTIATED)
			return SSL_TLSEXT_ERR_OK;

	return SSL_TLSEXT_ERR_NOACK;
};

using npn_cb = int (*) (SSL *ssl, const unsigned char** out, unsigned int* outlen, void *arg);
npn_cb npn_adv_cb = [](ssl_st *ctx, const unsigned char** out, unsigned int* outlen, void *arg)
{
	*out = npn_protos;
	*outlen = sizeof(npn_protos);
	return SSL_TLSEXT_ERR_OK;
};

void handler_factory::register_protocol_selection_callbacks(SSL_CTX* ctx)
{
	SSL_CTX_set_next_protos_advertised_cb(ctx, npn_adv_cb, nullptr);
	SSL_CTX_set_alpn_select_cb(ctx, alpn_select_cb, nullptr);
}


std::shared_ptr<http::server_connection> handler_factory::negotiate_handler(std::shared_ptr<ssl_socket> sck) const noexcept
{
	const unsigned char* proto{nullptr};
	unsigned int len{0};

	const SSL *ssl_handle = sck->native_handle();

	//ALPN
	SSL_get0_alpn_selected(ssl_handle, &proto, &len);
	if( !len )
		//NPN
		SSL_get0_next_proto_negotiated(ssl_handle, &proto, &len);


	handler_type type{handler_type::ht_h1};
	if( len )
	{
		LOGDEBUG("Protocol negotiated: ", std::string(reinterpret_cast<const char*>(proto), len));
		switch(len)
		{
		case 2:
		case 3:
		case 5:
			type = handler_type::ht_h2;
			break;
		}
	}

	http::proto_version version = (type == handler_type::ht_h2) ? http::proto_version::HTTP20 : ((proto == nullptr ||   proto[len-1] == '0') ? http::proto_version::HTTP10 : http::proto_version::HTTP11);

	return build_handler( type , version, sck);
}

} //namespace

#include "sni_solver.h"
#include "utils.h"
#include "log_wrapper.h"

#include "../service_locator/service_locator.h"
#include "../configuration/configuration_wrapper.h"

#include <fstream>
#include <openssl/ssl.h>
#include <boost/asio.hpp>
#include <boost/system/error_code.hpp>
#include <openssl/x509v3.h>
#include <iostream>

namespace ssl_utils
{

const char *const DEFAULT_CIPHER_LIST =
	"ECDHE-ECDSA-CHACHA20-POLY1305:ECDHE-RSA-CHACHA20-POLY1305:ECDHE-ECDSA-"
	"AES128-GCM-SHA256:ECDHE-RSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES256-GCM-"
	"SHA384:ECDHE-RSA-AES256-GCM-SHA384:DHE-RSA-AES128-GCM-SHA256:DHE-RSA-"
	"AES256-GCM-SHA384:ECDHE-ECDSA-AES128-SHA256:ECDHE-RSA-AES128-SHA256:ECDHE-"
	"ECDSA-AES128-SHA:ECDHE-RSA-AES256-SHA384:ECDHE-RSA-AES128-SHA:ECDHE-ECDSA-"
	"AES256-SHA384:ECDHE-ECDSA-AES256-SHA:ECDHE-RSA-AES256-SHA:DHE-RSA-AES128-"
	"SHA256:DHE-RSA-AES128-SHA:DHE-RSA-AES256-SHA256:DHE-RSA-AES256-SHA:ECDHE-"
	"ECDSA-DES-CBC3-SHA:ECDHE-RSA-DES-CBC3-SHA:EDH-RSA-DES-CBC3-SHA:AES128-GCM-"
	"SHA256:AES256-GCM-SHA384:AES128-SHA256:AES256-SHA256:AES128-SHA:AES256-"
	"SHA:DES-CBC3-SHA:!DSS";

void configure_tls_context_easy(SSL_CTX *ctx)
{
	auto ssl_opts = (SSL_OP_ALL & ~SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS) |
		SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_COMPRESSION |
		SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION |	SSL_OP_SINGLE_ECDH_USE |
		SSL_OP_NO_TICKET |	SSL_OP_CIPHER_SERVER_PREFERENCE;
	SSL_CTX_set_options(ctx, ssl_opts);
	SSL_CTX_set_mode(ctx, SSL_MODE_AUTO_RETRY);
	SSL_CTX_set_mode(ctx, SSL_MODE_RELEASE_BUFFERS);
	SSL_CTX_set_cipher_list(ctx, DEFAULT_CIPHER_LIST);

#ifndef OPENSSL_NO_EC
	auto ecdh = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
	if (ecdh) {
	SSL_CTX_set_tmp_ecdh(ctx, ecdh);
	EC_KEY_free(ecdh);
	}
#endif /* OPENSSL_NO_EC */

/*
	SSL_CTX_set_next_protos_advertised_cb( ctx,
	  [](SSL *s, const unsigned char **data, unsigned int *len, void *arg) {
		auto &token = get_alpn_token();

		*data = token.data();
		*len = token.size();

		return SSL_TLSEXT_ERR_OK;
	  },
	  nullptr);

#if OPENSSL_VERSION_NUMBER >= 0x10002000L
	// ALPN selection callback
	SSL_CTX_set_alpn_select_cb(ctx, alpn_select_proto_cb, nullptr);
#endif // OPENSSL_VERSION_NUMBER >= 0x10002000L
*/
}


bool sni_solver::load_certificates()
{
	/** This will load all certificates; however it cannot work, since we don't have enough info! */

	for(auto &t: raw_certificates)
	{
		if(!prepare_certificate(std::get<0>(t), std::get<1>(t), std::get<2>(t))) return false;
	}


	if(certificates_list.empty())
	{
		LOGWARN("could not load any certificate. HTTPS will not started.");
		return false;
	}

	return true;
}

bool sni_solver::prepare_certificate(std::string certificate_file, std::string key_file, std::string key_password)
{
	certificates_list.emplace_back("", boost::asio::ssl::context::tlsv12);
	auto& certificate = certificates_list.back();
	auto& context = certificate.context;
	configure_tls_context_easy( context.native_handle() );
	std::ifstream password_file;
	try
	{
		std::string decrypt_key_password = key_password;
		password_file.open( decrypt_key_password );
		std::string password;
		std::getline( password_file, password );
		password_file.close();
		context.set_password_callback(
			[ password ]( size_t maxLength, boost::asio::ssl::context::password_purpose purpose ) -> std::string
			{
				return password;
			} );
		context.use_private_key_file(key_file, boost::asio::ssl::context::pem);
		context.use_certificate_chain_file(certificate_file);
	}
	catch ( const boost::system::error_code& ec )
	{
		LOGWARN("error while setting the password callback");
		certificates_list.pop_back();
		return false;
	}
	catch ( const std::ifstream::failure& open_error )
	{
		certificates_list.pop_back();
		LOGWARN("could not open the password file, hence we cannot decrypt the certificate file");
		return false;
	} catch(...)
	{
		certificates_list.pop_back();
		return false;
	}



	//get server name from certificate
	SSL* ssl = SSL_new( context.native_handle());
	SSL_set_connect_state( ssl );
	X509* x509 = SSL_get_certificate( ssl );
	CRYPTO_add( &x509->references, 1, CRYPTO_LOCK_X509 );
	SSL_free( ssl );
	X509_NAME* subject = X509_get_subject_name( x509 );
	if ( !subject )
	{
		LOGWARN("could not get server name from certificate.");
		return false;
	}

	char cn[65];
	std::memset(cn, 0, 65);
	//int res = X509_NAME_get_text_by_NID( subject, NID_commonName, cn, 64 );
	cn[64] = '\0';
	auto ssl_ctx = context.native_handle();
	ssl_ctx->tlsext_servername_arg = this;
	certificate.server_name = std::string{ cn };
	certificate.x509 = x509;
	SSL_CTX_set_tlsext_servername_callback( ssl_ctx, sni_callback );
	return true;
}

static std::string clean_wildcard_name( const std::string& name )
{
	return name.substr( 2 ); // Clean up the first *.
}

int sni_callback( SSL* ssl, int* ad, void* arg )
{
	sni_solver* instance = reinterpret_cast<sni_solver*>(arg);
	if ( !instance )
	{
		LOGERROR("Argument for SNI_CALLBACK was not properly set. Please check the certificate decryption mechanism.");
		return SSL_TLSEXT_ERR_NOACK;
	}
	if ( ssl == nullptr )
		return SSL_TLSEXT_ERR_NOACK;
	auto server_name = SSL_get_servername( ssl, TLSEXT_NAMETYPE_host_name );
	if(server_name == nullptr) return SSL_TLSEXT_ERR_NOACK;
	const std::string required_server_name = std::string{server_name};
	/** The presence of these three loops is due to the fact that we want a priority
	 * between different matches:
	 * - in the first we check against the common name;
	 * - if no match, against wildcards
	 * - finally using subject alternative names extensions
	 * */
	for ( auto& certificate: instance->certificates_list )
	{
		if ( certificate.server_name == required_server_name )
		{
			//ok, found. SNI resolved.
			SSL_set_SSL_CTX( ssl, certificate.context.native_handle());

			//SSL_CTX_set_alpn_select_cb(certificate.context.native_handle(), );

			LOGINFO( "Found certificate: " + certificate.server_name );
			return SSL_TLSEXT_ERR_OK;
		}
	}

	// Wildcard certificates have only an * in the first position
	for ( auto& certificate: instance->certificates_list )
	{
		if ( ! ( certificate.server_name[0] == '*' &&  certificate.server_name[1] == '.' ) )
			continue;

		std::string server_name = clean_wildcard_name( certificate.server_name );
		if ( utils::ends_with ( required_server_name, server_name ) )
		{
			LOGINFO( "Found wildcard certificate: " + certificate.server_name );

			//ok, found. SNI resolved.
			SSL_set_SSL_CTX( ssl, certificate.context.native_handle());
			return SSL_TLSEXT_ERR_OK;
		}
	}

	for(auto &certificate: instance->certificates_list)
	{
		if(X509_check_host(certificate.x509, required_server_name.c_str(), required_server_name.length(), 0, NULL) == 1)
		{
			LOGINFO(" Found matching certificate by using x509v3 SAN extension ", required_server_name);
			SSL_set_SSL_CTX( ssl, certificate.context.native_handle());
			return SSL_TLSEXT_ERR_OK;
		}
	}
	LOGINFO("Could not find the indicated server name ", server_name ," cannot finish SNI.");
	return TLS1_AD_UNRECOGNIZED_NAME;
}

}

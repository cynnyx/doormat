#include "dns_communicator_factory.h"
#include "../../connector.h"
#include "../../../../../../../../usr/include/c++/6/chrono"
#include "../../../../../../../../usr/include/boost/system/error_code.hpp"

#include <chrono>

namespace network 
{

dns_connector_factory::dns_connector_factory(boost::asio::io_service& io, std::chrono::milliseconds connector_timeout)
	: io{io}
	, conn_timeout{connector_timeout}
	, dead{std::make_shared<bool>(false)}
{}

dns_connector_factory::~dns_connector_factory()
{
	*dead = true;
}

void dns_connector_factory::get_connector(const std::string& address, uint16_t port, bool tls,
	connector_callback_t connector_cb, error_callback_t error_cb)
{
	if ( stopping ) return error_cb(1);

	dns_resolver(address, port, tls, std::move(connector_cb), std::move(error_cb));
}

void dns_connector_factory::dns_resolver(const std::string& address, uint16_t port, bool tls,
	connector_callback_t connector_cb, error_callback_t error_cb)
{
	std::shared_ptr<boost::asio::ip::tcp::resolver> r = std::make_shared<boost::asio::ip::tcp::resolver>(io);
	if(!port)
		port = tls ? 443 : 80;

	LOGTRACE("resolving ", address, "with port ", port);
	boost::asio::ip::tcp::resolver::query q( address,  std::to_string(port) );
	auto resolve_timer =
		std::make_shared<boost::asio::deadline_timer>(io);
	resolve_timer->expires_from_now(boost::posix_time::milliseconds(resolve_timeout));
	resolve_timer->async_wait([r, resolve_timer](const boost::system::error_code &ec)
	{
		if ( ! ec ) r->cancel();
	});
	auto begin = std::chrono::high_resolution_clock::now();
	r->async_resolve(q,
		[this, begin, r, tls, connector_cb = std::move(connector_cb), error_cb = std::move(error_cb), resolve_timer, dead=dead]
		(const boost::system::error_code &ec, boost::asio::ip::tcp::resolver::iterator iterator)
		{
			if(*dead)
				return;
			resolve_timer->cancel();
			if(ec || iterator == boost::asio::ip::tcp::resolver::iterator())
			{
				LOGERROR(ec.message());
				return error_cb(3);
			}
			/** No error: go on connecting */
			LOGTRACE( "tls is: ", tls );
			if ( tls )
			{
				return endpoint_connect(std::move(iterator),
					std::make_shared<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>>
						(io, ctx ),
							std::move(connector_cb), std::move(error_cb));
			}
			endpoint_connect(std::move(iterator),
					std::make_shared<boost::asio::ip::tcp::socket>
						(io),
							std::move(connector_cb), std::move(error_cb));
		});
}

void dns_connector_factory::endpoint_connect(boost::asio::ip::tcp::resolver::iterator it, 
	std::shared_ptr<boost::asio::ip::tcp::socket> socket, connector_callback_t connector_cb, error_callback_t error_cb)
{

	if(it == boost::asio::ip::tcp::resolver::iterator() || stopping) //finished
		return error_cb(3);
	auto&& endpoint = *it;
	auto connect_timer = 
		std::make_shared<boost::asio::deadline_timer>(io);
	connect_timer->expires_from_now(boost::posix_time::milliseconds(connect_timeout));
	connect_timer->async_wait([socket, connect_timer, error_cb](const boost::system::error_code &ec)
	{
		if(!ec)
		{
			socket->cancel();
			error_cb(3);
		}
	});
	++it;
	socket->async_connect(endpoint,
		[this, it = std::move(it), socket, connector_cb = std::move(connector_cb), 
			error_cb = std::move(error_cb), connect_timer, dead=dead]
		(const boost::system::error_code &ec)
		{
			if(*dead)
				return;
			connect_timer->cancel();
			if ( ec)
			{
				if(ec != boost::system::errc::operation_canceled) {
						endpoint_connect(std::move(it), std::move(socket), std::move(connector_cb), std::move(error_cb));
				}
				return;
			}

			auto connector = std::make_shared<server::connector<server::tcp_socket>>(std::move(socket));
			connector->set_timeout(conn_timeout);
			connector_cb(std::move(connector), http::proto_version::HTTP11); // TODO: we are forcing http clear to http1.1
		});
}

http::proto_version dns_connector_factory::chose_protocol(std::shared_ptr<dns_connector_factory::ssl_socket_t> stream)
{
	const unsigned char* proto{nullptr};
	unsigned int len{0};
	const SSL *ssl_handle = stream->native_handle();

	//ALPN
	SSL_get0_alpn_selected(ssl_handle, &proto, &len);
	if( !len )
		//NPN
		SSL_get0_next_proto_negotiated(ssl_handle, &proto, &len);

	http::proto_version v = http::proto_version::HTTP10;
	if(len >= 2 && proto[0] == 'h' && proto[1] == '2')
		v = http::proto_version::HTTP20;
	else if(len == 8 && memcmp(proto, "http/1.1", 8) == 0)
		v = http::proto_version::HTTP11;

	return v;
}

void dns_connector_factory::endpoint_connect(boost::asio::ip::tcp::resolver::iterator it,
	std::shared_ptr<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>> stream, 
	connector_callback_t connector_cb, error_callback_t error_cb)
{
	if ( it == boost::asio::ip::tcp::resolver::iterator() || stopping ) //finished
		return error_cb(3);
	stream->set_verify_mode( boost::asio::ssl::verify_none );
	auto connect_timer =
		std::make_shared<boost::asio::deadline_timer>(io);
	connect_timer->expires_from_now(boost::posix_time::milliseconds(connect_timeout));
	connect_timer->async_wait([stream, connect_timer, error_cb](const boost::system::error_code &ec)
	{
		if ( !ec ) //stream->cancel();
		{
			boost::system::error_code sec;
			stream->shutdown( sec );
			stream->lowest_layer().cancel(sec);
			error_cb(3);
		}
	});
	boost::asio::async_connect( stream->lowest_layer(), it, [ this, stream, connector_cb = std::move(connector_cb),
		error_cb = std::move(error_cb), connect_timer, dead = dead ]( const boost::system::error_code &ec,
			boost::asio::ip::tcp::resolver::iterator it )
	{

		if(*dead)
			return;

		if ( ec )
		{
			LOGERROR(ec.message());
			//++it ?
			if(ec != boost::system::errc::operation_canceled)
				return endpoint_connect(std::move(it), std::move(stream), std::move(connector_cb), std::move(error_cb));
			return;
		}
		stream->async_handshake( boost::asio::ssl::stream_base::client, 
			[ this, stream, connector_cb = std::move(connector_cb), error_cb = std::move(error_cb), connect_timer ]
				( const boost::system::error_code &ec )
				{
					connect_timer->cancel();
					if ( ec )
					{
						LOGERROR( ec.message() );
						if(ec != boost::system::errc::operation_canceled) error_cb(3);

						return;
					}

					http::proto_version v = chose_protocol(stream);

					auto connector = std::make_shared<server::connector<server::ssl_socket>>(std::move(stream));
					connector->set_timeout(conn_timeout);
					connector_cb(std::move(connector), v);
				});
	});
}

namespace
{
static const std::string NGHTTP2_H2_ALPN = "\x2h2";
static const std::string NGHTTP2_H2_16_ALPN = "\x5h2-16";
static const std::string NGHTTP2_H2_14_ALPN = "\x5h2-14";
static const std::string HTTP1_1_ALPN = "\x8http/1.1";
static const std::string HTTP1_0_ALPN = "\x8http/1.0";

bool select_proto(const unsigned char **out, unsigned char *outlen,
				  const unsigned char *in, unsigned int inlen,
				  const std::string &key) {
  for (auto p = in, end = in + inlen; p + key.size() <= end; p += *p + 1) {
	if (std::equal(std::begin(key), std::end(key), p)) {
	  *out = p + 1;
	  *outlen = *p;
	  return true;
	}
  }
  return false;
}

bool select_h2(const unsigned char **out, unsigned char *outlen,
			   const unsigned char *in, unsigned int inlen)
{
  return select_proto(out, outlen, in, inlen, NGHTTP2_H2_ALPN) ||
		 select_proto(out, outlen, in, inlen, NGHTTP2_H2_16_ALPN) ||
		  select_proto(out, outlen, in, inlen, NGHTTP2_H2_14_ALPN);
}

bool select_h1(const unsigned char **out, unsigned char *outlen,
			   const unsigned char *in, unsigned int inlen)
{
  return select_proto(out, outlen, in, inlen, HTTP1_1_ALPN) ||
		 select_proto(out, outlen, in, inlen, HTTP1_0_ALPN);
}

int client_select_next_proto_cb(SSL *ssl, unsigned char **out,
								unsigned char *outlen, const unsigned char *in,
								unsigned int inlen, void *arg)
{
	if (!select_h2(const_cast<const unsigned char **>(out), outlen, in, inlen) &&
			!select_h1(const_cast<const unsigned char **>(out), outlen, in, inlen))
	{
		return SSL_TLSEXT_ERR_NOACK;
	}
	return SSL_TLSEXT_ERR_OK;
}

const std::vector<unsigned char>& get_default_alpn()
{
  static auto res = std::vector<unsigned char>(NGHTTP2_H2_ALPN.size() +
										NGHTTP2_H2_16_ALPN.size() +
										NGHTTP2_H2_14_ALPN.size() +
										HTTP1_1_ALPN.size() +
										HTTP1_0_ALPN.size());
  static bool initialized = false;
  if(!initialized)
  {
	  initialized = true;
	  auto p = std::begin(res);
	  p = std::copy_n(std::begin(NGHTTP2_H2_ALPN), NGHTTP2_H2_ALPN.size(), p);
	  p = std::copy_n(std::begin(NGHTTP2_H2_16_ALPN), NGHTTP2_H2_16_ALPN.size(), p);
	  p = std::copy_n(std::begin(NGHTTP2_H2_14_ALPN), NGHTTP2_H2_14_ALPN.size(), p);
	  p = std::copy_n(std::begin(HTTP1_1_ALPN), HTTP1_1_ALPN.size(), p);
	  p = std::copy_n(std::begin(HTTP1_0_ALPN), HTTP1_0_ALPN.size(), p);
  }

  return res;
}

} // unnamed namespace

boost::asio::ssl::context dns_connector_factory::init_ssl_ctx()
{
	boost::asio::ssl::context ctx{boost::asio::ssl::context::sslv23};
	// enable NPN for http2
	auto ctx_h = ctx.native_handle();
	SSL_CTX_set_next_proto_select_cb(ctx_h, client_select_next_proto_cb, nullptr);
	// enable ALPN for http2
	SSL_CTX_set_alpn_protos(ctx_h, get_default_alpn().data(), get_default_alpn().size());
	return ctx;
}

thread_local boost::asio::ssl::context dns_connector_factory::ctx{dns_connector_factory::init_ssl_ctx()};

}

#include "cloudia_pool.h"

#include "../service_locator/service_locator.h"
#include "../io_service_pool.h"
#include "../configuration/configuration_wrapper.h"
#include "../utils/log_wrapper.h"

#include <boost/asio.hpp>

namespace network
{

void cloudia_pool::get_socket(const http::http_request& req, socket_callback sc, error_callback ec )
{
	using namespace boost::asio;
	using namespace std;
	
	if ( stopping ) return;
	if ( ec ) on_error_cb = ec;
	on_socket = sc;
	
	auto&& io = service::locator::service_pool().get_thread_io_service();
	ip::tcp::resolver r( io );
	ip::tcp::resolver::query q( req.urihost(),  req.port() );
	
	boost::system::error_code error;
	it = r.resolve( q, error );
	if ( error )
		on_error();
	else if ( it !=  ip::tcp::resolver::iterator() )
	{
		init_socket();
		set_timeout();
		handle_connect( *it, sc );
	}		
}

void cloudia_pool::on_error()
{
	stopping = true;
	if ( on_error_cb )
		on_error_cb();
	else if ( on_socket )
		on_socket( std::unique_ptr<socket_type>{} );
}


void cloudia_pool::retry( socket_callback cb)
{
	using namespace boost::asio;
	
	if ( stopping ) return;
	if ( it == ip::tcp::resolver::iterator() ) return;
	
	if ( ++it != ip::tcp::resolver::iterator() ) 
		handle_connect( *it, cb );
	else
		on_error();
}

void cloudia_pool::init_socket()
{
	_socket.reset( new socket_type ( service::locator::service_pool().get_thread_io_service() ) );
}

void cloudia_pool::handle_connect( const boost::asio::ip::tcp::endpoint& endpoint, socket_callback cb )
{
	using namespace boost::asio;
		
	_socket->async_connect( endpoint,
		[cb, this](const boost::system::error_code &ec) mutable
	{
		if ( ! _socket->is_open() )
		{
			LOGERROR("TIMEOUT");
			retry( cb );
		}
		else if ( ec )
		{
			LOGERROR("error while establishing connection: ", ec.message());
			retry( cb );
		}
		else
		{
			if ( stopping )
			{
				_socket->close();
				on_error();
			}
			else
			{
				boost::system::error_code ec2;
				_deadline.cancel( ec2 );
				if ( ec2 ) LOGERROR( "Error cancelling timer? " );
	// 			socket_ptr->lowest_layer().set_option(boost::asio::ip::tcp::no_delay(true));
				cb( std::move( _socket ) );
			}
		}
	});
}

cloudia_pool::cloudia_pool(): _deadline{ service::locator::service_pool().get_thread_io_service() } {}

void cloudia_pool::get_socket(socket_callback sc )
{
	if ( stopping ) return;
	using namespace boost::asio;
	using namespace std;
	
	on_socket = sc;
	if ( it == ip::tcp::resolver::iterator() ) 
		on_error();
	else
	{
		init_socket();
		set_timeout();
		handle_connect ( *it, sc );
	}
}


void cloudia_pool::set_timeout()
{
	_deadline.expires_from_now ( boost::posix_time::milliseconds(
		service::locator::configuration().get_board_connection_timeout() ) );
	_deadline.async_wait([this](const boost::system::error_code &ec) mutable
	{
		if ( !ec ) 
		{
			boost::system::error_code ec2;
			//_socket->cancel( ec2 );
			//if ( ec2 ) { LOGERROR( "Error cancelling socket? " ); ec2 = {boost::system::error_code ec2}; }
			_socket->close( ec2 );
			if ( ec2 ) LOGERROR("Error closing socket! ");
		}
	});
}

bool cloudia_pool::set_destination( const std::string& ip, std::uint16_t port_, bool ssl )
{
	boost::system::error_code ec;
	auto endpoint = boost::asio::ip::tcp::endpoint{boost::asio::ip::address::from_string(ip, ec), port_};
	if ( !ec ) return false;
	it = boost::asio::ip::tcp::resolver::iterator::create( endpoint, ip, ssl ? "http" : "https" );
	return true;
}

void cloudia_pool::stop()
{
	stopping = true;
}

cloudia_pool::~cloudia_pool() = default;

std::unique_ptr<socket_factory> cloudia_pool_factory::get_socket_factory( std::size_t size ) const
{
	return std::unique_ptr<socket_factory>{ new cloudia_pool() };
}

}

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
	
	if ( stopping || ! handler ) return;
	if ( ec ) handler->on_error_cb = ec;
	handler->on_socket = sc;
	
	auto&& io = service::locator::service_pool().get_thread_io_service();
	ip::tcp::resolver r( io );
	ip::tcp::resolver::query q( req.urihost(),  req.port() );
	
	boost::system::error_code error;
	handler->it = r.resolve( q, error );
	if ( error )
		on_error();
	else if ( handler->it != ip::tcp::resolver::iterator() )
	{
		handler->init_socket();
		handler->set_timeout();
		handler->handle_connect( *(handler->it), sc );
	}		
}

cloudia_pool::connection_handler::connection_handler( cloudia_pool* out ): cp_p{out}, 
	_deadline{ service::locator::service_pool().get_thread_io_service() } { }

void cloudia_pool::on_error()
{
	stopping = true;
	if ( handler->on_error_cb )
		handler->on_error_cb();
	else if ( handler->on_socket )
		handler->on_socket( std::unique_ptr<socket_type>{} );
}


void cloudia_pool::connection_handler::retry( socket_callback cb)
{
	using namespace boost::asio;
	
	if ( ! cp_p || cp_p->stopping ) return;
	if ( it == ip::tcp::resolver::iterator() ) return;
	
	if ( it != ip::tcp::resolver::iterator() ) 
		handle_connect( *it, cb );
	else
		cp_p->on_error();
}

void cloudia_pool::connection_handler::init_socket()
{
	_socket.reset( new socket_type ( service::locator::service_pool().get_thread_io_service() ) );
}

void cloudia_pool::connection_handler::handle_connect( const boost::asio::ip::tcp::endpoint& endpoint, socket_callback cb )
{
	using namespace boost::asio;
	auto self  = shared_from_this();
	
	_socket->async_connect( endpoint,
		[cb, self](const boost::system::error_code &ec) mutable
	{
		if ( self->cp_p == nullptr )
		{
			LOGERROR("Gone!");
			self->_socket->close();
			boost::system::error_code ec2;
			self->_deadline.cancel( ec2 );
		}
		else if ( ! self->_socket->is_open() )
		{
			LOGERROR("TIMEOUT");
			self->retry( cb );
		}
		else if ( ec )
		{
			LOGERROR("error while establishing connection: ", ec.message());
			self->retry( cb );
		}
		else
		{
			if ( self->cp_p->stopping )
			{
				self->_socket->close();
				self->cp_p->on_error();
			}
			else
			{
				boost::system::error_code ec2;
				self->_deadline.cancel( ec2 );
				if ( ec2 ) LOGERROR( "Error cancelling timer? " );
	// 			socket_ptr->lowest_layer().set_option(boost::asio::ip::tcp::no_delay(true));
				cb( std::move( self->_socket ) );
			}
		}
	});
}

cloudia_pool::cloudia_pool()
{
	handler = std::shared_ptr<connection_handler>{ new connection_handler(this) };
}

void cloudia_pool::get_socket(socket_callback sc )
{
	if ( stopping && ! handler ) return;
	using namespace boost::asio;
	using namespace std;
	
	handler->on_socket = sc;
	if ( handler->it == ip::tcp::resolver::iterator() ) 
		on_error();
	else
	{
		handler->init_socket();
		handler->set_timeout();
		handler->handle_connect ( *(handler->it), sc );
	}
}

void cloudia_pool::connection_handler::set_timeout()
{
	_deadline.expires_from_now ( boost::posix_time::milliseconds(
		service::locator::configuration().get_board_connection_timeout() ) );
	auto self = shared_from_this();
	_deadline.async_wait([self](const boost::system::error_code &ec) mutable
	{
		if ( !ec ) 
		{
			boost::system::error_code ec2;
			self->_socket->close( ec2 );
			if ( ec2 ) LOGERROR("Error closing socket! ");
		}
	});
}

bool cloudia_pool::set_destination( const std::string& ip, std::uint16_t port_, bool ssl )
{
	boost::system::error_code ec;
	auto endpoint = boost::asio::ip::tcp::endpoint{boost::asio::ip::address::from_string(ip, ec), port_};
	if ( !ec ) return false;
	handler->it = boost::asio::ip::tcp::resolver::iterator::create( endpoint, ip, ssl ? "http" : "https" );
	return true;
}

void cloudia_pool::stop()
{
	stopping = true;
}

cloudia_pool::~cloudia_pool()
{
	if ( handler )
		handler->cp_p = nullptr;
}

std::unique_ptr<socket_factory<boost::asio::ip::tcp::socket>>
	cloudia_pool_factory::get_socket_factory( std::size_t size ) const
{
	return std::unique_ptr<cloudia_pool>{ new cloudia_pool() };
}

}

#ifndef DOOR_MAT_NODE_ERASED_H
#define DOOR_MAT_NODE_ERASED_H

#include <memory>
#include <utility>

#include "error_code.h"
#include "../protocol/handler_factory.h"
#include "../http/http_commons.h"
#include "../http/http_response.h"
#include "../protocol/handler_interface.h"

namespace http
{
class http_response;
}

// First implementation of type erasure for chain nodes!
// No ownership is taken by these classes - except the obvious ownership of internal states.
class node_erased final
{
	struct abstract_impl // Funny!
	{
		virtual ~abstract_impl() noexcept = default;
		virtual void on_header( http::http_response && ) = 0;
		virtual void on_body( dstring&& ) = 0;
		virtual void on_trailer( dstring&&, dstring&& ) = 0;
		virtual void on_end_of_message() = 0;
		virtual void on_error(const errors::error_code &ec) = 0;
	};

	template<class T>
	class impl : public abstract_impl
	{
		T& node;
		
		using type = 
			typename std::conditional<std::is_base_of<server::handler_interface, T>::value, std::false_type, std::true_type>::type;

		void call_end_of_message( std::true_type ) { node.on_end_of_message(); }
		void call_end_of_message( std::false_type ) { /* intentionally left blank*/ }
		
		void call_on_header( http::http_response && header, std::true_type ) { node.on_header( std::move( header ) ) ; }
		void call_on_header( http::http_response && header, std::false_type ) { /* intentionally left blank*/ }
		
		void call_on_body( dstring&& body, std::true_type ) { node.on_body( std::move( body ) ) ; }
		void call_on_body( dstring&& body, std::false_type ) { /* intentionally left blank*/ }
		
		void call_on_trailer( dstring&& key, dstring&& val, std::true_type ) 
		{
			node.on_trailer( std::move( key ), std::move( val ) );
		}
		void call_on_trailer( dstring&& key, dstring&& val, std::false_type ) { /* intentionally left blank*/ }
	public:
		impl(T& node_): node(node_) {}
		
		void on_header( http::http_response && header ) override {  call_on_header( std::move(header), type{} ); }
		void on_body( dstring&& body) override { call_on_body( std::move(body), type{} ); }
		void on_trailer( dstring&& key, dstring&& val ) override
		{
			call_on_trailer( std::move( key ), std::move( val ), type{} ); 
		}
		void on_end_of_message() override { call_end_of_message( type{} ); }
		void on_error(const errors::error_code &ec) override { node.on_error(ec); }
	};

	std::shared_ptr<abstract_impl> node;
public:
	// new and delete in our code - we will get rid of it!
	template<class T>
	node_erased( T& node_ ) { node = std::make_shared<impl<T>>( node_ ); }
	~node_erased() noexcept = default;

	node_erased( const node_erased& o ) { node = o.node; }
	node_erased& operator=( const node_erased& o ) { node = o.node; return *this; }

	void on_header( http::http_response && header ) { node->on_header( std::move(header ) ) ; }
	void on_body( dstring&& body) { node->on_body( std::move( body ) ); }
	void on_trailer( dstring&& key, dstring&& val ) { node->on_trailer( std::move( key ), std::move( val ) ); }
	void on_end_of_message() { node->on_end_of_message(); }
	void on_error(const errors::error_code &ec) { node->on_error(ec); }
};

#endif //DOOR_MAT_NODE_ERASED_H

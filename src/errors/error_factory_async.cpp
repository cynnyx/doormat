#include "error_factory_async.h"

#include "../service_locator/service_locator.h"
#include "../configuration/configuration_wrapper.h"
#include "../utils/resource_reader.h"
#include "../utils/log_wrapper.h"

#include <cstdint>
#include <utility>

namespace errors
{

void error_factory_async::start()
{
	assert( emf );
	emf->start();
}

error_factory_async::on_headers_type error_factory_async::get_on_header_callback() noexcept
{
	return [this] ( http::http_response& structured_data ) mutable
	{
		node.on_header( std::move ( structured_data ) );
	};
}

error_factory_async::on_body_type error_factory_async::get_on_body_callback() noexcept
{
	return [this] ( dstring& body ) mutable
	{
		node.on_body( std::move( body ) );
		node.on_end_of_message();
	};
}

error_factory_async::error_factory_async( node_erased& node_, std::uint16_t code_, http::proto_version proto  )
	: code(code_)
	, node(node_)
	, protocol(proto)
{
	std::string filename = service::locator::configuration().get_error_filename( code );

	detail::error_message_factory::callback_t cb = [this]( const utils::resource_reader::error_t& ec, std::string body_ )
	{
		// Taking ownership of the body!
		body = std::move(body_);
		std::size_t content_length{ body.length() };
		headers.status( code );
		headers.protocol( protocol );
		headers.content_len( content_length );
		headers.header( http::hf_content_type, http::content_type(http::content_type_t::text_html));
		headers.header( http::hf_content_type, http::hv_charset_utf8 );
		auto data = dstring{body.data(), body.size()};
		get_on_header_callback()( headers );
		get_on_body_callback()( data );
	};
	emf = std::make_shared<detail::error_message_factory>( filename, cb  );
}

std::shared_ptr<error_factory_async> error_factory_async::error_response( node_erased& node,
	std::uint16_t code, http::proto_version proto )
{
	std::shared_ptr<error_factory_async> refa{ new error_factory_async( node, code, proto ) };
	refa->start();
	return refa;
}

}

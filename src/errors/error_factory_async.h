#ifndef DOOR_MAT_ABSTRACT_ERROR_FACTORY_H
#define DOOR_MAT_ABSTRACT_ERROR_FACTORY_H

#include <string>
#include <list>
#include <memory>
#include "../chain_of_responsibility/node_erased.h"
#include "../http/http_response.h"
#include "error_messages.h"
#include "error_message_details.h"

namespace errors
{

// Inherit from this if you need a different set of callbacks
// You may need to work better with named constructor.
class error_factory_async
{
	std::uint16_t code;
	std::shared_ptr<detail::error_message_factory> emf;
	node_erased& node;
	http::http_response headers;
	std::string body;
	http::proto_version protocol;
public:
	virtual ~error_factory_async() noexcept = default;
	void start();

	error_factory_async( const error_factory_async& ) = delete;
	error_factory_async& operator=( error_factory_async& ) = delete;

	using on_body_type = std::function<void(dstring&)>;
	using on_headers_type = std::function<void(http::http_response&)>;

	static std::shared_ptr<error_factory_async> error_response( node_erased& node, std::uint16_t code, http::proto_version proto );
protected:
	error_factory_async( node_erased& node, std::uint16_t code, http::proto_version proto );
	virtual on_headers_type get_on_header_callback() noexcept;
	virtual on_body_type get_on_body_callback() noexcept;
};

}

#endif //DOOR_MAT_ABSTRACT_ERROR_FACTORY_H

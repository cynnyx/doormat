#include "error_message_details.h"

#include "../service_locator/service_locator.h"
#include "../configuration/configuration_wrapper.h"

#include "cynnypp/async_fs.hpp"

namespace detail
{

namespace filesystem = cynny::cynnypp::filesystem;

error_message_factory::error_message_factory(const std::string& file_path, detail::error_message_factory::callback_t cb)
	: utils::resource_reader(file_path, std::bind(&error_message_factory::handle_chunk, this, std::placeholders::_1, std::placeholders::_2),
	std::bind(&error_message_factory::handle_completion, this, std::placeholders::_1, std::placeholders::_2)), cb(std::move(cb))
{
	data.reserve(1024);
}

void error_message_factory::handle_chunk(error_message_factory* ptr, const utils::resource_reader::error_t& ec,
	utils::resource_reader::buffer_t chunk)
{
	ptr->data.insert(ptr->data.end(), chunk.begin(), chunk.end());
}

void error_message_factory::handle_completion(error_message_factory* ptr, const error_t& ec, size_t bytes_read) noexcept
{
	try
	{
		if ( ec )
			return ptr->cb(ec, {});

		// call the callback with success
		ptr->cb(error_t::success, ptr->do_body());
	}
	catch(...)
	{
		//FIXME Adding logging here
		ptr->invoke_callback_with_error();
	}
}

std::string error_message_factory::do_body()
{
	replace_placeholder(data);
	return std::move(data); // WARNING: can be used only once
}

void error_message_factory::replace_placeholder(std::string& str)
{
	static const std::string placeholder = "REPLACETHISPLEASEPLEASEREPLACETHISPLEASEPLEASE";

	size_t pos{str.size()};
	while( std::string::npos != (pos = str.rfind(placeholder, pos)) )
		str.replace(pos, placeholder.size(), service::locator::configuration().get_error_host());
}

}

#include "resource_reader.h"

#include <string>

#include "../service_locator/service_locator.h"
#include "../fs/fs_manager_wrapper.h"
#include "log_wrapper.h"



namespace utils
{

namespace filesystem = cynny::cynnypp::filesystem;

static const std::string log_tag = "[Resource reader] ";

resource_reader::resource_reader(const std::string& file_path, chunk_callback_t chunk_cb, done_callback_t done_cb)
	: reader(service::locator::fs_manager().make_chunked_stream(file_path))
	, chunk_cb(std::move(chunk_cb))
	, done_cb(std::move(done_cb))
	, total_bytes(0)
{}

void resource_reader::start()
{
	reader->next_chunk(std::bind(&resource_reader::read_handler, shared_from_this(), std::placeholders::_1, std::placeholders::_2));
}


void resource_reader::read_handler(std::shared_ptr<resource_reader> shared, const error_t& ec, filesystem::Buffer buf) noexcept
{
	try
	{
		if(!buf.empty())
		{
			auto sz = buf.size();
			shared->chunk_cb(ec, std::move(buf));
			shared->total_bytes += sz;
		}

		if(ec)
			return shared->completion_handler(ec);

		shared->reader->next_chunk(std::bind(&resource_reader::read_handler, shared, std::placeholders::_1, std::placeholders::_2));
	}
	catch(...)
	{
		return shared->invoke_callback_with_error();
	}
}

void resource_reader::completion_handler(const resource_reader::error_t& ec) noexcept
{
	try
	{
		if(ec != error_t::end_of_file)
			return done_cb(ec, total_bytes);

		// call the callback with success
		return done_cb(error_t::success, total_bytes);
	}
	catch(...)
	{
		return invoke_callback_with_error();
	}
}

void resource_reader::invoke_callback_with_error() noexcept
{
	try
	{
		done_cb(error_t::internal_failure, total_bytes);
	}
	catch(...)
	{
		LOGERROR(log_tag , "An error occurred while reading a resource");
	}
}


} // namespace utils

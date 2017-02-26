#include "fs_manager_wrapper.h"

#include "../service_locator/service_locator.h"
#include "../io_service_pool.h"


using namespace cynny::cynnypp::filesystem;


fs_manager_wrapper::fs_manager_wrapper()
	: FilesystemManager{io_}
	, work_{new boost::asio::io_service::work(io_)}
	, thrd_{[this]{ io_.run(); }}
{

}

fs_manager_wrapper::~fs_manager_wrapper()
{
	work_.reset();
	thrd_.join();
}

void fs_manager_wrapper::async_read(const Path& p, Buffer& buf, CompletionHandler h)
{
	auto &destination_io_service = service::locator::service_pool().get_thread_io_service();
	auto ch = [&destination_io_service, h](const ErrorCode &ec, size_t bytesRead)
	{
		destination_io_service.post(std::bind(h, ec, bytesRead));
	};
	FilesystemManager::async_read(p, buf, std::move(ch));
}


void fs_manager_wrapper::async_read(std::unique_ptr<std::basic_ifstream<uint8_t>> fd, Buffer &buf, CompletionHandler h)
{
	auto &destination_io_service = service::locator::service_pool().get_thread_io_service();
	auto ch = [&destination_io_service, h](const ErrorCode &ec, size_t bytesRead)
	{
		destination_io_service.post(std::bind(h, ec, bytesRead));
	};
	FilesystemManager::async_read(std::move(fd), buf, std::move(ch));
}

void fs_manager_wrapper::async_write(const Path& p, const Buffer& buf, CompletionHandler h) {
	auto &destination_io_service = service::locator::service_pool().get_thread_io_service();
	auto ch = [&destination_io_service, h](const ErrorCode &ec, size_t bytesRead){
		destination_io_service.post( std::bind(h, ec, bytesRead));
	};
	FilesystemManager::async_write(p, buf, std::move(ch));
}

void fs_manager_wrapper::async_append(const Path& p, const Buffer& buf, CompletionHandler h)
{
	auto &destination_io_service = service::locator::service_pool().get_thread_io_service();
	auto ch = [&destination_io_service, h](const ErrorCode &ec, size_t bytesRead)
	{
		destination_io_service.post(std::bind(h, ec, bytesRead));
	};
	FilesystemManager::async_append(p, buf, std::move(ch));
}

void fs_manager_wrapper::async_read_chunk(std::shared_ptr<ChunkedReader> r, size_t pos, BufferView& buf, CompletionHandler h)
{
	auto &destination_io_service = service::locator::service_pool().get_thread_io_service();
	auto ch = [&destination_io_service, h](const ErrorCode &ec, size_t bytesRead)
	{
		destination_io_service.post(std::bind(h, ec, bytesRead));
	};
	FilesystemManager::async_read_chunk(std::move(r), pos, buf, std::move(ch));
}

std::shared_ptr<ChunkedFstreamInterface> fs_manager_wrapper::make_chunked_stream(const Path& p, size_t chunk_size)
{
	return std::make_shared<wrapped_chunked_stream>(*this, p, chunk_size);
}

wrapped_chunked_stream::wrapped_chunked_stream(fs_manager_wrapper& ref, const Path& p, size_t chunk_size)
{
	impl = ref.make_old_fashioned_chunked_stream(p, chunk_size);
}

void wrapped_chunked_stream::next_chunk(FilesystemManagerInterface::ReadChunkHandler h)
{
	auto &destination_io_service = service::locator::service_pool().get_thread_io_service();
	auto rch = [&destination_io_service, h](const ErrorCode& e, Buffer b)
	{
		destination_io_service.post(std::bind(h, e, std::move(b)));
	};
	impl->next_chunk(std::move(rch));
}

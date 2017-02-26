#ifndef DOOR_MAT_FS_MANAGER_WRAPPER_H
#define DOOR_MAT_FS_MANAGER_WRAPPER_H


#include <boost/asio.hpp>
#include <cynnypp/async_fs.hpp>

#include <thread>
#include <memory>


class fs_manager_wrapper : public cynny::cynnypp::filesystem::FilesystemManager
{
	using FilesystemManager = cynny::cynnypp::filesystem::FilesystemManager;
	using FilesystemManagerInterface = cynny::cynnypp::filesystem::FilesystemManagerInterface;
	using ChunkedFstreamInterface = cynny::cynnypp::filesystem::ChunkedFstreamInterface;
	using ChunkedReader = cynny::cynnypp::filesystem::impl::ChunkedReader;
	using Path = cynny::cynnypp::filesystem::Path;
	using BufferView = cynny::cynnypp::filesystem::impl::HotDoubleBuffer::BufferView;
	using Buffer = cynny::cynnypp::filesystem::Buffer;
	using ErrorCode = cynny::cynnypp::filesystem::ErrorCode;

	friend class wrapped_chunked_stream;

	std::shared_ptr<ChunkedFstreamInterface> make_old_fashioned_chunked_stream(const Path& p, size_t chunk_size = cynny::cynnypp::filesystem::pageSize)
	{
		return FilesystemManager::make_chunked_stream(p, chunk_size);
	}

	boost::asio::io_service io_;
	std::unique_ptr<boost::asio::io_service::work> work_;
	std::thread thrd_;

public:
	fs_manager_wrapper();
	~fs_manager_wrapper();

	//--------------------------------------------- asynchronous interface : now we will have fun!

	/**
	 * Register an asynch read request to the fs manager.
	 *
	 * The request will be enqueued on the task queue of the fs manager and the read operation will be performed
	 * on the worker thread as soon as all previous pending tasks will have been completed.
	 *
	 * It's very important to note that the caller should never pass a filesystem I/O operation
	 * inside the completion handler. In fact, the CompletionHandler run on the "main/application" thread,
	 * while all I/O fs operation should be enqueued on the worker thread of the fs manager, so to not create
	 * contention on the file system.
	 *
	 * \param p - the path to the file to be read
	 * \param buf - the buffer where the caller want the read data to be deposited
	 * \param h - completion handler for the read operation (must be convertible to FilesystemManager::CompletionHandler)
	 */
	void async_read(const Path &p, Buffer &buf, CompletionHandler h) override;


	void async_read(std::unique_ptr<std::basic_ifstream<uint8_t>> fd, Buffer &buf, CompletionHandler h) override;

	/**
	 * Register an asynch write request to the fs manager.
	 *
	 * The request will be enqueued on the task queue of the fs manager and the write operation will be performed
	 * on the worker thread as soon as all previous pending tasks will have been completed.
	 *
	 * It's very important to note that the caller should never pass a filesystem I/O operation
	 * inside the completion handler. In fact, the CompletionHandler run on the "main/application" thread,
	 * while all I/O fs operation should be enqueued on the worker thread of the fs manager, so to not create
	 * contention on the file system.
	 *
	 * \param p - the path to the file to be written
	 * \param buf - the buffer containing the data to be written to fs
	 * \param h - completion handler for the write operation (must be convertible to FilesystemManager::CompletionHandler)
	 */
	void async_write(const Path &p, const Buffer &buf, CompletionHandler h) override;

	/**
	* Register an asynch append request to the fs manager, overload.
	*
	* \param p - the path to the file to be written
	* \param buf - the buffer containing the data to be appended
	* \param h - completion handler for the append operation (must be convertible to FilesystemManager::CompletionHandler)
	*/
	void async_append(const Path&p, const Buffer &buf, FilesystemManagerInterface::CompletionHandler h) override;

	/** Reads a chunk asynchronously, using a chunked reader
	 * \param r the chunkedreader to be used to perform the read
	 * \param pos the position from which the read starts
	 * \param buf the buffer to be used to save the data
	 * \param h the completion handler to be called on read termination.
	 */
	void async_read_chunk(std::shared_ptr<ChunkedReader>r, size_t pos, BufferView& buf, CompletionHandler h) override;

	std::shared_ptr<ChunkedFstreamInterface> make_chunked_stream(const Path& p, size_t chunk_size = cynny::cynnypp::filesystem::pageSize) override;
};


class wrapped_chunked_stream : public cynny::cynnypp::filesystem::ChunkedFstreamInterface {
public:
	using Path = cynny::cynnypp::filesystem::Path;
	using Buffer = cynny::cynnypp::filesystem::Buffer;
	using ErrorCode = cynny::cynnypp::filesystem::ErrorCode;
	using FilesystemManagerInterface = cynny::cynnypp::filesystem::FilesystemManagerInterface;

	wrapped_chunked_stream(fs_manager_wrapper &ref, const Path& p, size_t chunk_size);

	void next_chunk(FilesystemManagerInterface::ReadChunkHandler h)  override;

private:
	std::shared_ptr<ChunkedFstreamInterface> impl{nullptr};
};

#endif //DOOR_MAT_FS_MANAGER_WRAPPER_H

#ifndef DOORMAT_RESOURCE_READER_H_
#define DOORMAT_RESOURCE_READER_H_

#include <memory>
#include <functional>
#include <vector>

namespace cynny
{
namespace cynnypp
{
namespace filesystem
{
class ErrorCode;
using Buffer = std::vector<uint8_t>;
struct ChunkedFstreamInterface;
}
}
}

namespace utils
{

/**
 * @brief The resource_reader class asynchronously read a resource (usually a file)
 * and returns the content of every chunk through callback. Another callback is
 * called when the read operations are concluded, with or without errors.
 *
 * The read is performed using a cynny::cynnypp::filesystem::ChunkedFstreamInterface
 * returned from the \ref service::locator filesystem manager.
 */
class resource_reader : public std::enable_shared_from_this<resource_reader>
{
public:
	using error_t = cynny::cynnypp::filesystem::ErrorCode;
	using buffer_t = cynny::cynnypp::filesystem::Buffer;
	using chunk_callback_t = std::function<void(const error_t& ec, buffer_t buf)>;
	using done_callback_t = std::function<void(const error_t& ec, size_t total_size)>;

	resource_reader(const std::string& file_path, chunk_callback_t chunk_cb, done_callback_t done_cb);
	void start();

protected:
	void completion_handler(const error_t& ec) noexcept;
	void invoke_callback_with_error() noexcept;

private:
	using reader_t = cynny::cynnypp::filesystem::ChunkedFstreamInterface;
	using reader_ptr_t = std::shared_ptr<reader_t>;

	static void read_handler(std::shared_ptr<resource_reader> shared, const error_t& ec, buffer_t buf) noexcept;

	reader_ptr_t reader;
	chunk_callback_t chunk_cb;
	done_callback_t done_cb;
	size_t total_bytes;
};


} // namespace utils


#endif // DOORMAT_RESOURCE_READER_H_

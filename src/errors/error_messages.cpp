#include <memory>

#include "error_messages.h"
#include "error_message_details.h"

#include "cynnypp/async_fs.hpp"

namespace errors
{

void error_body(const std::string& file_path, std::function<void(const cynny::cynnypp::filesystem::ErrorCode&, std::string)> on_msg_ready)
{
	auto factory = std::make_shared<detail::error_message_factory>(file_path, on_msg_ready );
	factory->start();
}

} // namespace errors

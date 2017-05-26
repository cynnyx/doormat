#include "mock_handler.h"

void mock_handler::do_write()
{}

void mock_handler::on_connector_nulled()
{}

bool mock_handler::start() noexcept
{
	return false;
}

bool mock_handler::should_stop() const noexcept
{
	return false;
}

bool mock_handler::on_read(const char*, unsigned long)
{
	return false;
}

bool mock_handler::on_write(std::string& chunk)
{
	return true;
}

void mock_handler::trigger_timeout_event()
{
	timeout_cb();
}

std::vector<std::pair<std::function<void()>, std::function<void()>>> mock_handler::write_feedbacks()
{
	return std::vector<std::pair<std::function<void()>, std::function<void()>>>();
}

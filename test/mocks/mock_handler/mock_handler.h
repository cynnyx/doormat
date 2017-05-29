#pragma once

#include "src/protocol/http_handler.h"

class connector_interface;

class mock_handler : public server::http_handler
{
public:
	using success_or_error_collbacks = std::vector<std::pair<std::function<void(void)>, std::function<void(void)>>>;

	mock_handler(const std::function<void(void)> &timeout_cb = [](){},
	             const success_or_error_collbacks& cbs = success_or_error_collbacks{});

	bool start() noexcept override;
	bool should_stop() const noexcept override;
	bool on_read(const char *, unsigned long) override;
	bool on_write(std::string &chunk) override;
	void trigger_timeout_event() override;
	success_or_error_collbacks write_feedbacks() override;

private:
	void do_write() override;

	void on_connector_nulled() override;

	std::function<void(void)> timeout_cb;
	success_or_error_collbacks cbs;
};

#pragma once

#include "../src/service_locator/service_initializer.h"
#include "../src/chain_of_responsibility/chain_of_responsibility.h"
#include "../src/network/cloudia_pool.h"
#include "../src/io_service_pool.h"

#include <gtest/gtest.h>
#include <boost/optional.hpp>

namespace test_utils
{

namespace preset
{

/// @note if this grow up in parameters let's use a fluent builder!
void setup( configuration::configuration_wrapper* cw, logging::inspector_log* il = nullptr );
void teardown();
void teardown( std::unique_ptr<node_interface>& chain );
void init_thread_local();
void stop_thread_local();

struct mock_conf : public configuration::configuration_wrapper
{
	uint32_t get_thread_number() const noexcept override
	{
		return 1;
	}

	std::string get_route_map() const noexcept override
	{
		static constexpr auto rm = "./etc/doormat/route_map_mock";
		return rm;
	}

	std::string get_error_host() const override
	{
		static constexpr auto eh = "https://errors.tcynny.com:1443";
		return eh;
	}

	std::string get_error_filename( uint16_t code ) const override
	{
		 return "./etc/doormat/pages/403.html";
	}

	std::string get_path() const override
	{
		return "./etc/doormat/pages";
	}

	bool privileged_ipv4_matcher(const std::string& ip) const noexcept override
	{
		return ip == "127.0.0.1";
	}

	bool interreg_ipv4_matcher(const std::string& ip) const noexcept override
	{
		return ip == "127.0.0.1";
	}

	uint16_t get_port() const noexcept override
	{
		return 443;
	}

	std::string get_log_path() const noexcept override
	{
		return "./dummy_log_dir/";
	}

	size_t get_max_reqsize() const noexcept override
	{
		return 1024;
	}

	uint64_t get_board_timeout() const noexcept override
	{
		return 100;
	}


	bool cache_enabled() const noexcept override {
		return true;
	}

	std::string cache_path() const noexcept override
	{
		return "/tmp/cache/";
	}

	const std::vector<std::pair<bool, std::string>>& get_cache_domains_config() const noexcept override
	{
		return cache_domain_config;
	}

	static void set_cache_domains_config(std::vector<std::pair<bool, std::string>> v) noexcept
	{
		cache_domain_config = std::move(v);
	}


	bool get_compression_enabled() const noexcept override { return compression_enabled; }


	static void set_compression(bool enabled) { compression_enabled = enabled; }


	bool comp_mime_matcher(const std::string&) const noexcept override { return true; }
	uint8_t get_compression_level() const noexcept override { return 9; }
	uint32_t get_compression_minsize() const noexcept override { return 0; }

private:
	static std::vector<std::pair<bool, std::string>> cache_domain_config;
	static bool compression_enabled;
};

struct test : public ::testing::Test
{
	std::unique_ptr<node_interface> ch;
protected:
	virtual void SetUp() override
	{
		preset::setup(new mock_conf{});
	}

	virtual void TearDown() override
	{
		preset::teardown(ch);
	}
};
}

struct first_node : node_interface
{
	using node_interface::node_interface;

	first_node() {}
	~first_node() noexcept { reset(); }

	static boost::optional<http::http_request> request;
	static boost::optional<http::http_response> response;
	static uint req_body;
	static uint req_trailer;
	static uint req_eom;
	static uint res_body;
	static uint res_trailer;
	static uint res_eom;
	static uint res_continue;
	static std::string res_body_str;
	static errors::error_code err;
	static std::function<void(void)> res_start_fn;
	static std::function<void(void)> res_stop_fn;
	static void reset();

	void on_request_preamble(http::http_request&& r);
	void on_request_body(dstring&& c);
	void on_request_trailer(dstring&& k, dstring&& v);
	void on_request_finished();

	void on_header(http::http_response&& r);
	void on_body(dstring&&);
	void on_trailer(dstring&&, dstring&&);
	void on_end_of_message();
	void on_error(const errors::error_code& ec);
	void on_response_continue();
};

struct last_node : node_interface
{
	using node_interface::node_interface;

	last_node() {}
	~last_node() noexcept { reset(); }

	static boost::optional<http::http_request> request;
	static uint req_body;
	static uint req_trailer;
	static uint req_eom;
	static errors::error_code err;
	static void reset();

	void on_request_preamble(http::http_request&& r);
	void on_request_body(dstring&& c);
	void on_request_trailer(dstring&& k, dstring&& v);
	void on_request_finished();
	void on_request_canceled(const errors::error_code& ec);

	static boost::optional<http::http_response> response;
	static std::list<dstring> body_parts;
	static dstring trailer_key;
	static dstring trailer_value;
};

struct blocked_node : node_interface
{
	using node_interface::node_interface;

	blocked_node() {}

	void on_request_preamble(http::http_request&&){ FAIL(); }
	void on_request_body(dstring&&) { FAIL(); }
	void on_request_trailer(dstring&&, dstring&&) { FAIL(); }
	void on_request_finished(){ FAIL(); }
};

}

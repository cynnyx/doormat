#ifndef CONFIGURATIONWRAPPER_H
#define CONFIGURATIONWRAPPER_H

#include <map>
#include <vector>
#include <list>
#include <string>
#include <exception>
#include <memory>

#include "header_configuration.h"
#include "../IPV4Network.h"

/**
 * NEVER USE THIS HEADER DIRECTLY
 * PASS IT THROUGH SERVICE LOCATOR
 */

namespace configuration
{

class certificates_iterator;

class configuration_wrapper
{
	friend class configuration_maker;
	friend class certificates_iterator;

	struct certificate_config
	{
		std::string certificate_file;
		std::string key_file;
		std::string key_password;
		bool is_default{false};

		void reset() noexcept;
		bool is_complete() const noexcept;
	};

	std::map<uint16_t, std::string> error_filenames;
	std::vector<certificate_config> certificates;
	std::vector<std::pair<bool, std::string>> cache_domains{};
	std::list<network::IPV4Network> privileged_networks;
	std::list<network::IPV4Network> interreg_networks;
	std::string route_map;
	std::string path;
	std::string log_path;
	std::string error_host;
	uint32_t threads{0};
	int32_t port{-1};
	int32_t porth{-1};

	uint64_t client_connection_timeout{ 2000L }; // How much we wait to connect to a client
	uint64_t board_connection_timeout { 1000L };
	uint64_t operation_timeout{ 26000L }; // Transaction timeout - client side
	uint64_t board_timeout{ 26000L }; // Transaction timeout - board side
	uint8_t max_connection_attempts{3};


	void parse_network_file(const std::string&, std::list<network::IPV4Network>&);
	bool http2_disabled{false};
	bool http2_next{false};
	bool inspector{false};
	bool daemonize{false};
	std::string daemon_path{""};

	std::string log_level{"info"};

	bool cache_enabled_{false};
	std::string cache_path_{""};

	size_t size_limit{0};

	std::unique_ptr<header> header_conf{ std::unique_ptr<header>( new header ) };
	bool cache_allowed;

	void parse_mime_types(const std::string& str, std::vector<std::string>& types );
	bool comp_enabled{false};
	uint8_t comp_level{0};
	uint32_t comp_minsize{0};
	std::vector<std::string> compressed_mime_types;

	std::string magnet_metadata_map{""};
	std::string magnet_data_map{""};
	size_t fd_limit{0};

protected:
	configuration_wrapper() = default;
public:
	virtual ~configuration_wrapper() = default;

	virtual bool privileged_ipv4_matcher(const std::string&) const noexcept;
	virtual bool interreg_ipv4_matcher(const std::string&) const noexcept;
	virtual bool comp_mime_matcher(const std::string&) const noexcept;

	virtual bool http2_is_disabled() const noexcept{ return http2_disabled; }
	virtual bool http2_ng() const noexcept { return http2_next; }
	virtual bool inspector_active() const noexcept { return inspector; }
	virtual std::string get_error_filename( uint16_t code ) const;
	virtual std::string get_route_map() const noexcept { return route_map; }
	virtual std::string get_path() const { return path; }
	virtual std::string get_log_path() const noexcept { return log_path; }
	virtual std::string get_log_level() const noexcept { return log_level; }
	virtual std::string get_error_host() const { return error_host; }
	virtual uint16_t get_port() const noexcept;
	virtual uint16_t get_port_h() const noexcept;
	virtual uint32_t get_thread_number() const noexcept { return threads; }
	virtual uint64_t get_client_connection_timeout() const noexcept;
	virtual uint64_t get_board_connection_timeout() const noexcept { return board_connection_timeout; }
	virtual uint64_t get_operation_timeout() const noexcept;
	virtual uint64_t get_board_timeout() const noexcept;
	virtual size_t get_max_reqsize() const noexcept { return size_limit; }
	virtual const header& header_configuration() const noexcept;
	virtual std::string get_daemon_root() const noexcept { return daemon_path; }
	virtual bool cache_enabled() const noexcept { return cache_enabled_; }
	virtual std::string cache_path() const noexcept { return cache_path_; }
	virtual const std::vector<std::pair<bool, std::string>>& get_cache_domains_config() const noexcept { return cache_domains; }
	virtual bool get_compression_enabled() const noexcept { return comp_enabled; }
	virtual uint8_t get_compression_level() const noexcept { return comp_level; }
	virtual uint32_t get_compression_minsize() const noexcept { return comp_minsize; }
	virtual uint8_t get_max_connection_attempts() const noexcept { return max_connection_attempts; }
	virtual size_t get_fd_limit() const noexcept { return fd_limit; }

	certificates_iterator iterator() const;
	void set_port_maybe(int32_t forced_port);
	void set_fd_limit(const size_t fdl) noexcept;
	void set_thread_number(const uint32_t& t) noexcept { threads = t; }
	void set_log_level(const std::string& ll) noexcept { log_level = ll; }
};

/**
 * @warning This object is mutable so it's not thread safe
 */
class certificates_iterator final
{
	const configuration_wrapper* wrapper;
	mutable std::uint32_t index;
public:
	certificates_iterator( const configuration_wrapper* w = nullptr);

	const certificates_iterator& next() const
	{
		++index;
		return *this;
	}

	void reset() const noexcept
	{
		index = 0;
	}

	std::string certificate_file() const noexcept
	{
		if ( is_valid() )
			return wrapper->certificates[ index ].certificate_file;
		return std::string("");
	}

	std::string key_file() const noexcept
	{
		if ( is_valid() )
			return wrapper->certificates[ index ].key_file;
		return std::string("");
	}

	std::string key_password() const noexcept
	{
		if ( is_valid() )
			return wrapper->certificates[ index ].key_password;
		return std::string("");
	}

	bool is_default() const noexcept
	{
		if ( is_valid() )
			return wrapper->certificates[ index ].is_default;
		return false;
	}

	bool is_valid() const noexcept
	{
		return wrapper != nullptr && index < wrapper->certificates.size();
	}

	bool is_production() const noexcept
	{
		return is_valid() && wrapper->certificates[ index ].certificate_file.find("production") != std::string::npos;
	}
};

}
#endif // CONFIGURATIONWRAPPER_H

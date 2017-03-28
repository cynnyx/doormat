#ifndef DOORMAT_CONFIGURATION_MAKER_H
#define DOORMAT_CONFIGURATION_MAKER_H

#include "../utils/json.hpp"
#include "../utils/log_wrapper.h"
#include <string>
#include <bitset>
#include "../../deps/cynnypp/include/cynnypp/async_fs.hpp"
#include "configuration_wrapper.h"


namespace configuration 
{

class configuration_maker 
{
	using json = nlohmann::json;
public:
	configuration_maker(bool verbose);

	void set_tabs(const std::string &tabs) { this->tabs = &tabs; }

	bool accept_setting(const json &setting);

	bool is_configuration_valid() /* everything was inserted*/
	{
		return mandatory_inserted.count() == mandatory_inserted.size();
	}

	std::unique_ptr<configuration_wrapper> get_configuration()
	{
		if(!is_configuration_valid())
		{
			std::string missing_fields;
			for(size_t i = 0; i < mandatory_inserted.size(); ++i)
			{
				if(mandatory_inserted.test(i) == false)
				{
					missing_fields+=mandatory_keys[i]+" ";
				}
			}
			throw std::logic_error{"Not all mandatory fields in configuration have been initialized. Missing fields are: " 
				+ missing_fields};
		}
		return std::move(cw);
	}

	~configuration_maker() = default;

private:
	template<typename... Args>
	void notify(Args&&... args)
	{
		if(!verbose) return;
		std::cout << *tabs << "[CONFIGURATION MAKER] " << utils::stringify(std::forward<Args>(args)...) << std::endl;
	}

	void notify_valid()
	{
		notify("found valid configuration for key ", current_key, "[OK]");
	}

	//configuration_wrapper wrp;
	const static std::string mandatory_keys[13];
	const static std::string allowed_keys[15];

	std::bitset<sizeof(mandatory_keys)/sizeof(std::string)> mandatory_inserted;
	std::unique_ptr<configuration_wrapper> cw;
	bool verbose;
	const std::string *tabs{nullptr};
	std::string current_key{};

	/** A little bit faster verifiers*/
	bool is_number_integer(const json &js);

	bool is_array(const json &js);

	bool is_object(const json &js);

	bool is_string(const json &js);

	bool is_boolean(const json &js);

	bool add_configuration(const std::string &key, const json &js);

	bool certificate_configuration(const json &js);

	bool routemap_configuration(const json &js);

	bool pa_configuration(const json &js);

	bool pagebase_configuration(const json &js);

	bool port_configuration(const json &js);

	bool porth_configuration(const json &js);

	bool cconnectiontimeout_configuration(const json &js);

	bool bconnectiontimeout_configuration(const json &js);

	bool boardtimeout_configuration(const json &js);

	bool logpath_configuration(const json &js);

	bool threads_configuration(const json &js);

	bool interregaddress_configuration(const json &js);

	bool rsizelimit_configuration(const json &js);

	bool headerconfig_configuration(const json &js);

	bool disablehttp2_configuration(const json &js);

	bool daemon_configuration(const json &js);

	bool loglevel_configuration(const json &js);

	bool inspector_active_configuration( const json& js );

	bool cache_configuration(const json &js);

	bool gzip_configuration(const json &js);

	bool errorhost_configuration(const json &js);

	bool operationtimeout_configuration(const json &js);

	bool errorfiles_configuration(const json &js);

	bool fd_configuration(const json &js);

	bool connectionattempts_configuration(const json &js);

	bool cache_normalization_configuration(const json &js);

	bool magnet_configuration(const json &js);
};

}

#endif //DOORMAT_CONFIGURATION_MAKER_H

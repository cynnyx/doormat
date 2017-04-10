#ifndef DOORMAT_CONFIGURATION_MAKER_H
#define DOORMAT_CONFIGURATION_MAKER_H

#include "../utils/json.hpp"
#include "../utils/log_wrapper.h"
#include "abstract_configuration_maker.h"
#include <memory>
#include <vector>


namespace configuration 
{

class configuration_wrapper;


class doormat_proxy_configuration_maker : public abstract_configuration_maker
{
	using json = nlohmann::json;
public:
    doormat_proxy_configuration_maker(bool verbose); //dpme
	doormat_proxy_configuration_maker(bool verbose, configuration_wrapper * cw); //done

	bool is_configuration_valid() override /* everything was inserted*/
	{
        size_t mandatory_inserted_count{0};
        for(const auto&&m: mandatory_inserted) { mandatory_inserted_count += m; }
        return mandatory_inserted_count == mandatory_keys.size();
	}

	virtual ~doormat_proxy_configuration_maker() = default;

private:

    bool is_mandatory(const std::string& str) override;
    bool is_allowed(const std::string&str) override;
    void set_mandatory(const std::string&key) override;

	//configuration_wrapper wrp;
	const static std::vector<std::string> mandatory_keys;
	const static std::vector<std::string> allowed_keys;

	std::vector<bool> mandatory_inserted; //fix this, it is a k
	bool verbose;
	const std::string *tabs{nullptr};


    bool add_configuration(const std::string &key, const json &js) override;

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

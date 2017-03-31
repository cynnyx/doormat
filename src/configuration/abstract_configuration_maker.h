#ifndef DOORMAT_ABSTRACT_CONFIGURATION_MAKER_H
#define DOORMAT_ABSTRACT_CONFIGURATION_MAKER_H

#include "../utils/json.hpp"
#include "../utils/log_wrapper.h"
#include "configuration_wrapper.h"

namespace configuration {

class abstract_configuration_maker {
    using json = nlohmann::json;
public:
    abstract_configuration_maker(bool verbose, configuration_wrapper *cw);

    /** Common methods */
    void set_tabs(const std::string &tabs) { this->tabs = &tabs; }
    bool accept_setting(const json &setting);

    bool is_number_integer(const json &js);

    bool is_array(const json &js);

    bool is_object(const json &js);

    bool is_string(const json &js);

    bool is_boolean(const json &js);

    /** pure virtual methods*/
    std::unique_ptr<configuration_wrapper> get_configuration();
    virtual bool is_configuration_valid() = 0;

    virtual ~abstract_configuration_maker()=default;


protected:


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




    virtual bool is_mandatory(const std::string& str)=0;
    virtual bool is_allowed(const std::string&str)=0;
    virtual void set_mandatory(const std::string&key)=0;
    virtual bool add_configuration(const std::string &key, const json &js)=0;

    const std::string *tabs{nullptr};
    std::string current_key{};
    bool verbose;
    std::unique_ptr<configuration_wrapper> cw;
};

}




#endif //DOORMAT_ABSTRACT_CONFIGURATION_MAKER_H

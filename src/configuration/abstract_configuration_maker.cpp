#include "abstract_configuration_maker.h"
#include <string>
namespace configuration
{


abstract_configuration_maker::abstract_configuration_maker(bool verbose, configuration_wrapper *cw) : verbose{verbose} , cw{cw} {}

bool abstract_configuration_maker::accept_setting(const json &setting) {
    for(auto begin = setting.cbegin(); begin != setting.cend(); ++begin) {
        std::string key = begin.key();
        if(is_mandatory(key)) {
            notify("mandatory key \"", key, "\" retrieved.");
            if(add_configuration(key, begin.value())) {
                set_mandatory(key);
                return true;
            }
            throw std::logic_error{"configuration for key " + key + " is invalid."};
        }
        if(is_allowed(key)) {
            notify("allowed key \"", key, "\" retrieved.");
            if (add_configuration(key, begin.value())) {
                return true;
            }
            throw std::logic_error{"configuration for key " + key + " is invalid"};
        }
        notify("key \"", key, "\" not recognized as valid. please check your spelling.");
        LOGDEBUG("[Configuration] unrecognized option ", key);
    }
    return false;
}

bool abstract_configuration_maker::is_number_integer(const json &js)
{
    if(!js.is_number_integer())
    {
        std::string js_rep = js;
        notify("key \"", current_key, "\" expected as value an integer number. Provided ", js_rep, " instead");
        return false;
    }
    return true;
}


bool abstract_configuration_maker::is_array(const json& js)
{

    if(!js.is_array())
    {
        std::string js_rep = js;
        notify("key \"", current_key, "\" expected as value an array. Provided ", js_rep, " instead");
        return false;
    }
    return true;
}


bool abstract_configuration_maker::is_object(const json &js)
{
    if(!js.is_object())
    {
        notify("key \"", current_key, "\" expected as value an object. Provided something different instead");
        return false;
    }
    return true;
}


bool abstract_configuration_maker::is_boolean(const json &js)
{
    if(!js.is_boolean())
    {
        std::string js_rep = js;
        notify("key \"", current_key, "\" expected as value a boolean. Provided ", js_rep, " instead");
        return false;
    }
    return true;
}


bool abstract_configuration_maker::is_string(const json &js)
{
    if(!js.is_string())
    {
        std::string js_rep = js;
        notify("key \"", current_key, "\" expected as value a string. Provided ", js_rep, " instead");
        return false;
    }
    return true;
}

std::unique_ptr<configuration_wrapper>abstract_configuration_maker::get_configuration()
{
    if(!is_configuration_valid())
    {
        std::string missing_fields;
        /*for(size_t i = 0; i < mandatory_inserted.size(); ++i)
        {
            if(mandatory_inserted[i] == false)
            {
                missing_fields+=mandatory_keys[i]+" ";
            }
        } todo: reintroduce it. */
        throw std::logic_error{"Not all mandatory fields in configuration have been initialized. Missing fields are: "
                               + missing_fields};
    }
    return std::move(cw);
}


}

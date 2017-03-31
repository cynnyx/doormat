#include "doormat_proxy_configuration_maker.h"
#include "configuration_wrapper.h"

#include <cynnypp/async_fs.hpp>
#include <algorithm>


using cynny::cynnypp::filesystem::exists;


namespace configuration
{

const std::vector<std::string> doormat_proxy_configuration_maker::mandatory_keys
{
	"route_map", "privileged_addresses", "page_base", "port", "porth", "client_connection_timeout",
	"board_connection_timeout",
	"operation_timeout", "board_timeout", "log_path", "error_host", "error_files"
};

const std::vector<std::string> doormat_proxy_configuration_maker::allowed_keys
{
	"threads","interreg_address","request_size_limit","header_config","disable_http2",
	"daemon", "inspector",
	"log_level","cache", "gzip", "connection_attempts", "file_descriptor_limit", "cache_normalization", "magnet", "certificates"
};


doormat_proxy_configuration_maker::doormat_proxy_configuration_maker(bool verbose, configuration_wrapper *cw) : abstract_configuration_maker{verbose, cw} {
    mandatory_inserted.clear();
    mandatory_inserted.resize(30, false);
}


doormat_proxy_configuration_maker::doormat_proxy_configuration_maker(bool verbose) : abstract_configuration_maker{verbose, new configuration_wrapper()}
{
    mandatory_inserted.clear();
	mandatory_inserted.resize(30, false);
}

bool doormat_proxy_configuration_maker::add_configuration(const std::string &key, const json &js)
{
	/*"certificate", "route_map", "privileged_addresses", "page_base", "port", "porth", "connection_timeout",
	"operation_timeout", "board_timeout", "log_path"
		 * Mandatory: DONE
		 * */
	current_key = key;
	if (key == "certificates") return certificate_configuration(js);
	if (key == "route_map") return routemap_configuration(js);
	if (key == "privileged_addresses") return pa_configuration(js);
	if (key == "page_base")return pagebase_configuration(js);
	if (key == "port") return port_configuration(js);
	if (key == "porth") return porth_configuration(js);
	if (key == "client_connection_timeout") return cconnectiontimeout_configuration(js);
	if (key == "board_connection_timeout") return bconnectiontimeout_configuration(js);
	if (key == "operation_timeout") return operationtimeout_configuration(js);
	if (key == "board_timeout") return boardtimeout_configuration(js);
	if (key == "log_path") return logpath_configuration(js);
	if (key == "error_host") return errorhost_configuration(js);
	if (key == "error_files") return errorfiles_configuration(js);
	/** Allowed:
	 * threads, interreg_address, request_size_limit, header_config, disable_http2, daemon,
	 * log_level, cache_path, cached_domains, gzip[compression_level,  compression_min_size, compressed_mime_types]
	 * magnet
	 */
	if (key == "threads") return threads_configuration(js);
	if (key == "interreg_address") return interregaddress_configuration(js);
	if (key == "request_size_limit") return rsizelimit_configuration(js);
	if (key == "header_config") return headerconfig_configuration(js);
	if (key == "disable_http2") return disablehttp2_configuration(js);
	if (key == "inspector") return inspector_active_configuration( js );
	if (key == "daemon") return daemon_configuration(js);
	if (key == "log_level") return loglevel_configuration(js);
	if (key == "cache") return cache_configuration(js);
	if (key == "gzip") return gzip_configuration(js);
	if (key == "connection_attempts") return connectionattempts_configuration(js);
	if (key == "file_descriptor_limit") return fd_configuration(js);
	if (key == "cache_normalization") return cache_normalization_configuration(js);
	if (key == "magnet") return magnet_configuration(js);

	return false;
}


bool doormat_proxy_configuration_maker::cache_normalization_configuration(const json&js)
{
	if(!is_array(js))
		return false;

	for(auto rule = js.cbegin(); rule != js.cend(); ++rule)
	{

		if(!is_object(rule.value()))
		{
			notify("cache_normalization directive must have as value an array of objects with mandatory fields vhost and cache_key, and allowed fields path and user_agent");
			return false;
		}
		bool has_vhost = false;
		bool has_cache_key = false;
		std::string vhost, cache_key; //mandatory
		std::string path = "(.*)";
		std::string user_agent = "(.*)";
		for(auto rule_field = rule.value().cbegin(); rule_field != rule.value().cend(); ++rule_field)
		{
			if(rule_field.key() == "vhost")
			{
				if(!is_string(rule_field.value())) return false;
				has_vhost = true;
				vhost = rule_field.value();
			} else if(rule_field.key() == "cache_key")
			{
				if(!is_string(rule_field.value())) return false;
				cache_key = rule_field.value();
				has_cache_key = true;
			} else if(rule_field.key() == "path")
			{
				if(!is_string(rule_field.value())) return false;
				path = rule_field.value();
			} else if(rule_field.key() == "user_agent")
			{
				if(!is_string(rule_field.value())) return false;
				user_agent = rule_field.value();
			} else
			{
				notify("key ", rule_field.key(), " not allowed in cache_normalization rule.");
				return false;
			}
		}
		if(!has_vhost || !has_cache_key)
		{
			notify("every cache_normalization rule must contain mandatory fields \"vhost\" and \"cache_key\"", has_vhost, has_cache_key);
			return false;
		}

		notify_valid();
	}
	return true;
}

bool doormat_proxy_configuration_maker::operationtimeout_configuration(const json &js)
{

	if(!is_number_integer(js)) return false;
	cw->operation_timeout = js;
	notify_valid();
	return true;
}


bool doormat_proxy_configuration_maker::connectionattempts_configuration(const json &js)
{
	if(!is_number_integer(js) || static_cast<int64_t>(js) >= 256)
	{
		notify("key \"", current_key, "\" allows an integer not greather than 256");
		return false;
	}
	cw->max_connection_attempts = js;
	notify_valid();
	return true;
}



bool doormat_proxy_configuration_maker::certificate_configuration(const json &js)
{
	if (!is_array(js)) return false;
	int default_count = 0;
	for (auto cbegin = js.cbegin(); cbegin != js.cend(); ++cbegin)
	{
		auto &curjson = *cbegin;
		if (!is_object(curjson)) return false;
		bool has_certificate_file = false;
		bool has_key_file = false;
		//bool has_key_pwd = false;
		//bool is_default = false;

		std::string certificate_file;
		std::string key_file;
		std::string key_pwd;
		configuration_wrapper::certificate_config certificate;
		for (auto innerbegin = curjson.cbegin(); innerbegin != curjson.cend(); ++innerbegin)
		{

			if (innerbegin.key() == "certificate_file")
			{
				if (!is_string(innerbegin.value())) return false;
				has_certificate_file = true;
				certificate_file = innerbegin.value();
				if (!exists(certificate_file))
					throw std::logic_error{"Certificate file" + certificate_file + " does not exist"};
				certificate.certificate_file = certificate_file;
			}
			if (innerbegin.key() == "key_file")
			{
				if (!is_string(innerbegin.value())) return false;
				has_key_file = true;
				key_file = innerbegin.value();
				if (!exists(key_file)) throw std::logic_error{"Certificate key File " + key_file + " does not exist"};
				certificate.key_file = key_file;
			}
			if (innerbegin.key() == "key_password")
			{
				if (!is_string(innerbegin.value())) return false;
				//has_key_pwd = true;
				key_pwd = innerbegin.value();
				if (!exists(key_pwd))
					throw std::logic_error{"Key password " + key_pwd + " does not exist"};
				certificate.key_password = key_pwd;
			}
			if (innerbegin.key() == "default")
			{
				if (!is_boolean(innerbegin.value()) || (innerbegin.value() && default_count > 0))
				{
					notify("only one 'default' field with value true is allowed for the certificates. ");
					return false;
				}
				bool is_default = innerbegin.value();
				if (is_default) ++default_count;;
				certificate.is_default = is_default;
			}
			/** if it is there, add it to configuration_wrapper.*/
		}
		/** Add to configuration wrapper*/
		if (has_certificate_file && has_key_file)
		{
			if (certificate.is_default)
				cw->certificates.insert(cw->certificates.begin(), certificate);
			else
				cw->certificates.push_back(certificate);
		} else {
			notify("every certificate must have a 'certificate_file' field and a 'key_file' one");
			return false;
		}
	}
	notify_valid();
	return true;
}

bool doormat_proxy_configuration_maker::routemap_configuration(const json &js)
{
	if (!is_string(js)) return false;
	std::string route_map = js;
	if (!exists(js)) throw std::logic_error{"route map file " + route_map + " does not exist"};
	cw->route_map = route_map;
	notify_valid();notify_valid();
	return true;
}

bool doormat_proxy_configuration_maker::pa_configuration(const json &js)
{
	if (!is_string(js)) return false;
	std::string pa = js;
	if (!exists(pa)) throw std::logic_error{"priviledged addresses file " + pa + " does not exist"};
	cw->parse_network_file(pa, cw->privileged_networks);
	notify_valid();
	return true;
}

bool doormat_proxy_configuration_maker::pagebase_configuration(const json &js)
{
	if (!is_string(js)) return false;
	std::string pb = js;
	if (!exists(pb)) throw std::logic_error{"page base direcotry " + pb + " does not exist"};
	cw->path = pb;
	notify_valid();
	return true;
}

bool doormat_proxy_configuration_maker::port_configuration(const json &js)
{
	if (!is_number_integer(js)) return false;
	int port = js;
	if (port <= 0 || port >= 65536)
		throw std::logic_error{"invalid port number " + std::to_string(port) + "in the configuration file"};
	cw->port = port;
	notify_valid();
	return true;
}

bool doormat_proxy_configuration_maker::porth_configuration(const json &js)
{
	if (!is_number_integer(js)) return false;
	int port = js;
	if (port <= 0 || port >= 65536)
		throw std::logic_error{"invalid porth number " + std::to_string(port) + "in the configuration file"};
	cw->porth = port;
	return true;
}

bool doormat_proxy_configuration_maker::cconnectiontimeout_configuration(const json &js)
{
	if (!is_number_integer(js)) return false;
	long int timeout = js;
	if (timeout <= 0) throw std::logic_error{"invalid timeout of " + std::to_string(timeout) + "milliseconds"};
	cw->client_connection_timeout = timeout;
	notify_valid();
	return true;
}

bool doormat_proxy_configuration_maker::bconnectiontimeout_configuration(const json &js)
{
	if (!is_number_integer(js)) return false;
	long int timeout = js;
	if (timeout <= 0) throw std::logic_error{"invalid timeout of " + std::to_string(timeout) + "milliseconds"};
	cw->board_connection_timeout = timeout;
	notify_valid();
	return true;
}

bool doormat_proxy_configuration_maker::boardtimeout_configuration(const json &js) {
	if (!is_number_integer(js)) return false;
	long int timeout = js;
	if (timeout <= 0) throw std::logic_error{"invalid board timeout of" + std::to_string(timeout) + " seconds"};
	cw->board_timeout = timeout;
	notify_valid();
	return true;
}

bool doormat_proxy_configuration_maker::logpath_configuration(const json &js)
{
	if (!is_string(js)) return false;
	cw->log_path = js;
	notify_valid();
	return true;
}

bool doormat_proxy_configuration_maker::threads_configuration(const json &js)
{
	if (!is_number_integer(js)) return false;
	int threads = js;
	if(threads < 1) throw std::logic_error("threads number " + std::to_string(threads) + " is invalid");
	cw->threads = threads;
	notify_valid();
	return true;
}


bool doormat_proxy_configuration_maker::interregaddress_configuration(const json &js)
{
	if (!is_string(js)) return false;
	std::string interregg_file = js;
	if (!exists(interregg_file)) throw std::invalid_argument("invalid interreg ips file " + interregg_file);
	cw->parse_network_file(interregg_file, cw->interreg_networks);
	notify_valid();
	return true;
}

bool doormat_proxy_configuration_maker::rsizelimit_configuration(const json &js)
{
	if (!is_number_integer(js)) return false;
	long int limit = js;
	cw->size_limit = limit;
	notify_valid();
	return true;
}

bool doormat_proxy_configuration_maker::headerconfig_configuration(const json &js)
{
	if(!is_string(js)) return false;
	std::string headerconfigfile = js;
	if(!exists(headerconfigfile)) throw std::logic_error{"invalid header configuration file " + headerconfigfile};
	cw->header_conf = std::unique_ptr<header> { new header(headerconfigfile) };
	notify_valid();
	return true;
}

bool doormat_proxy_configuration_maker::inspector_active_configuration( const json& js )
{
	if(!is_boolean(js)) return false;
	cw->inspector = js;
	notify_valid();
	return true;
}

bool doormat_proxy_configuration_maker::disablehttp2_configuration(const json &js)
{
	if(!is_boolean(js)) return false;
	cw->http2_disabled = js;
	notify_valid();
	return true;
}

bool doormat_proxy_configuration_maker::daemon_configuration(const json &js)
{
	if(!is_string(js)) return false;
	std::string daemon_path = js;
	cw->daemonize = true;
	cw->daemon_path = daemon_path;
	notify_valid();
	return true;
}


bool doormat_proxy_configuration_maker::loglevel_configuration(const json &js)
{
	if(!is_string(js)) return false;
	std::string ll = js;
	if(ll != "fatal" && ll != "error" && ll != "debug" && ll != "trace" && ll != "warn" && ll != "info")
		throw std::logic_error{"Invalid log level " + ll + " in configuration file"};
	cw->log_level = ll;
	notify_valid();
	return true;
}


bool doormat_proxy_configuration_maker::cache_configuration(const json &js)
{
	if(!is_object(js)) return false;

	bool has_cache_path = false;
	std::string cache_path;
	std::vector<std::pair<bool,std::string>> domains;
	for(auto cb = js.cbegin(); cb != js.cend(); ++cb)
	{
		if(cb.key() == "path")
		{
			has_cache_path = true;
			if(!is_string(cb.value())) return false;
			cache_path = cb.value();
		}

		if(cb.key() == "domains")
		{
			if(!is_array(cb.value())) return false;
			auto domainsarray = cb.value();
			for(auto domain_iterator = domainsarray.cbegin(); domain_iterator != domainsarray.cend(); ++domain_iterator)
			{
				if(!is_object(domain_iterator.value())) return false;
				auto &obj = *domain_iterator;
				if(!is_boolean(obj.cbegin().value())) return false;
				domains.emplace_back(bool(obj.cbegin().value()), std::string(obj.cbegin().key()));
			}
		}

	}
	cw->cache_enabled_ = true;
	cw->cache_path_ = cache_path;
	cw->cache_domains = domains;
	if(has_cache_path) notify_valid();
	return has_cache_path;
}



bool doormat_proxy_configuration_maker::gzip_configuration(const json &js)
{
	if(!js.is_object()) return false;
	int compression_level = -1;
	int compression_min_size = 0;
	std::vector<std::string> compressed_mime_types;
	for(auto b = js.cbegin(); b != js.cend(); ++b)
	{
		if(b.key() == "compression_level")
		{
			if(!is_number_integer(b.value())) return false;
			compression_level = b.value();
			if(compression_level < 0 || compression_level > 9)
				throw std::logic_error{"Invalid gzip compression level "+ std::to_string(compression_level) + " in configuration file"};
			continue;
		}
		if(b.key() == "compression_mime_types")
		{
			if(!is_array(b.value())) return false;
			for(auto mimes = b.value().cbegin(); mimes != b.value().cend(); ++mimes)
			{
				if(!is_string(mimes.value())) return false;
				std::string mime = mimes.value();
				compressed_mime_types.push_back(mime);
			}
			continue;
		}

		if(b.key() == "compression_min_size")
		{
			if(!is_number_integer(b.value()))
				return false;
			compression_min_size = b.value();
			if(compression_min_size < 0)
				return false;
			continue;
		}

	}

	cw->comp_enabled = true;
	cw->comp_level = compression_level;
	cw->comp_minsize = compression_min_size;
	cw->compressed_mime_types.swap(compressed_mime_types);

	notify_valid();
	return compression_level >= 0;
}


bool doormat_proxy_configuration_maker::errorhost_configuration(const json &js)
{
	if(!is_string(js)) return false;
	cw->error_host = js;
	notify_valid();
	return true;
}


bool doormat_proxy_configuration_maker::errorfiles_configuration(const json &js)
{
	if(!is_array(js)) return false;
	for(auto pair = js.cbegin(); pair != js.cend(); ++pair)
	{
		auto obj = pair.value().cbegin();
		std::string errcode = obj.key();
		int actual_errcode = 0;
		try
		{
			actual_errcode = std::stoi(errcode);
			if(actual_errcode < 400 || actual_errcode > 999) throw std::invalid_argument{""};
		} catch(const std::invalid_argument &invalid_conversion)
		{
			throw std::logic_error{"Wrong http error code specified for error_files: "+ errcode};
		}

		if(!is_string(obj.value())) return false;

		std::string filename = obj.value();

		/** Store as the file name!*/
		cw->error_filenames.emplace(actual_errcode, filename);
	}
	notify_valid();
	return true;
}


bool doormat_proxy_configuration_maker::fd_configuration(const json &js)
{
	if(!js.is_number_unsigned())
	{
		std::string js_rep = js;
		notify("key \"", current_key, "\" expected as value an unsigned integer number. Provided ", js_rep, " instead");
		return false;
	}
	cw->set_fd_limit(js);
	notify_valid();
	return true;
}

bool doormat_proxy_configuration_maker::magnet_configuration(const json &js)
{
	if(!js.is_object())
	{
		std::string js_rep = js;
		notify("key \"", current_key, "\" expected as value an object. Provided ", js_rep, " instead");
		return false;
	}

	try
	{
		cw->magnet_data_map = js.value("data_map", "");
		cw->magnet_metadata_map = js.value("metadata_map", "");
		if(cw->magnet_data_map.empty() && cw->magnet_metadata_map.empty())
		{
			notify("data_map or metadata_map must be enabled in order to use magnet.");
			return false;
		}

	} catch(...)
	{
		notify("could not find correct configuration for magnet");
		return false;
	}

	return true;

}

bool doormat_proxy_configuration_maker::is_mandatory(const std::string &str) {
    auto el_iter = std::find(mandatory_keys.begin(), mandatory_keys.end(), str);
    return el_iter != mandatory_keys.end();
}

void doormat_proxy_configuration_maker::set_mandatory(const std::string &key) {
    auto el_iter = std::find(mandatory_keys.begin(), mandatory_keys.end(), key);
    if(el_iter != mandatory_keys.end()) {
        auto pos = std::distance(mandatory_keys.begin(), el_iter);
        mandatory_inserted[pos] = true;
    }
}


bool doormat_proxy_configuration_maker::is_allowed(const std::string &str) {
    auto el_iter = std::find(allowed_keys.begin(), allowed_keys.end(), str);
    return el_iter != allowed_keys.end();
}


}

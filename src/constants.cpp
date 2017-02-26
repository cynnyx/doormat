#include "constants.h"

const std::string& project_name()
{
	static const std::string ret{"Doormat"};
	return ret;
}

const std::string& version()
{
	static const std::string ret{VERSION};
	return ret;
}

const std::string& sys_subdomain_start()
{
	static const std::string ret{"sys."};
	return ret;
}

const std::string& interreg_subdomain_start()
{
	static const std::string ret{"interreg."};
	return ret;
}

const std::string& admin_subdomain_start()
{
	static const std::string ret{"franco."};
	return ret;
}

const std::string& board_reset_header()
{
	static const std::string ret{"Cyn-Board-Reset"};
	return ret;
}

uint8_t max_retries()
{
	return 5;
}

uint default_timeout()
{
	return 5000;
}

uint reserved_fds()
{
	return 30;
}

bool http10()
{
	return false;
}

bool randomization()
{
	return true;
}

bool debug()
{
	return true;
}

#ifndef _CONSTANTS__H_
#define _CONSTANTS__H_

#include <string>

/**
 * Here there are only declarations - for configuration default values go to the cpp
 *
 * Use this stuff only for real CONSTANT values - if they are configurable use the configuration object.
 *
 **/

/** \brief returns the name of the project
 * */
const std::string& project_name();

/** \brief returns project's version
 * */
const std::string& version();

/** \brief returns system subdomain prefix.
 * */
const std::string& sys_subdomain_start();

/** \brief returns inter-region subdomain prefix.
 * */
const std::string& interreg_subdomain_start();

/** \brief returns administrative subdomain prefix.
 * */
const std::string& admin_subdomain_start();

/** \brief returns the header used to toggle a single board in failure back to working state.
 * */
const std::string& board_reset_header();

/** \brief returns the maximum connection attempts allowed.
 * */
uint8_t max_retries();

/** \brief get default timeout in ms
 * */
uint default_timeout();

/** \brief get number of reserved FDs
 * */
uint reserved_fds();

//FIXME:what's that worth for??
/** \brief check it http 1.0
 * */
bool http10();

/** \brief
 * */
bool randomization();

/** \brief check if debug's mode on
 * */
bool debug();

#endif

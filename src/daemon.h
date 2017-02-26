#ifndef _DAEMON__H_
#define _DAEMON__H_

#include <string>

extern bool watchdog;

#ifdef _GNU_SOURCE
void jailify();
#endif

void becoming_deamon(const std::string& root);
void stop_daemon();

#endif

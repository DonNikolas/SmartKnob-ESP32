#pragma once
#include <map>
#include <string>
#include <time.h>
#include <stdint.h>
extern std::map<std::string, int32_t> hostNvs;
extern bool hostNvsOpen, hostNvsWrite, hostPwmAttach;
extern int hostWrites, hostWifiState, hostReconnects, hostNtpStarts;
extern uint32_t hostDuty;
extern time_t hostEpoch;

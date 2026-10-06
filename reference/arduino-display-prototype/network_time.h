#ifndef KNOB_NETWORK_TIME_H
#define KNOB_NETWORK_TIME_H
#include <time.h>

void network_time_init();
void network_time_poll();
bool network_time_configured();
bool network_time_connected();
// Non-blocking. False until the system clock contains a plausible date.
bool network_time_local(struct tm *result);
#endif

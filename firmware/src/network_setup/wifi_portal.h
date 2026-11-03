#pragma once
namespace KnobWifiPortal {
// Both Preferences operations must run with all motor outputs disabled.
bool consumeRequest();
bool requestNextBoot();
// Dedicated setup boot: starts no GUI/network/motor workers. Returns via reset.
void run();
}

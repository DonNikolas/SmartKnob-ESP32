#pragma once
#if __has_include("home_feeds_private/config.h")
#include "home_feeds_private/config.h"
#else
#include "home_feeds_config_defaults.h"
#endif
#include <string.h>
namespace KnobHomeConfig {
static_assert(sizeof(URLS) / sizeof(URLS[0]) == 5, "Configure five HTTP pages");
inline const char* urlFor(int index) {
    return index >= 0 && index < 5 && URLS[index] ? URLS[index] : "";
}
inline bool configured(int index) {
    const char* url = urlFor(index);
    return strncmp(url, "https://", 8) == 0 && url[8] != '\0';
}
}

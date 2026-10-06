#pragma once
#include <stdint.h>
#include <stddef.h>
class Preferences {
public:
    bool begin(const char *name, bool readOnly);
    void end();
    int32_t getInt(const char *key, int32_t defaultValue);
    size_t putInt(const char *key, int32_t value);
};

#ifndef KNOB_SETTINGS_H
#define KNOB_SETTINGS_H

bool settings_init();
int settings_load(const char *key, int defaultValue, int minValue, int maxValue, int step);
// True only if already stored or successfully committed to NVS.
bool settings_save(const char *key, int value);
#endif

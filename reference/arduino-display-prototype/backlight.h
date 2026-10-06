#ifndef KNOB_BACKLIGHT_H
#define KNOB_BACKLIGHT_H

bool backlight_init();
void backlight_set_percent(int percent, bool smooth = true);
void backlight_poll();
int backlight_target_percent();
#endif

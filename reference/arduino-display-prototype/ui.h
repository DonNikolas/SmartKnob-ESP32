#ifndef UI_H
#define UI_H
#include "input.h"
void ui_init();
void ui_handle_input(InputEvent evt);
void ui_check_idle();
void ui_notify_activity();
#endif

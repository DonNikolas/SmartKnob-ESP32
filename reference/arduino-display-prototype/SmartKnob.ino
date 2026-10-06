#include "config.h"
#include "display.h"
#include "input.h"
#include "ui.h"
#include "settings.h"
#include "network_time.h"
#include "backlight.h"

void setup() {
    Serial.begin(115200);
    settings_init();

    if (!display_init()) {
        while (true) delay(100); // Never call LVGL with an uninitialized display.
    }
    input_init();
    ui_init();
    network_time_init();
}

void loop() {
    network_time_poll();
    InputEvent evt = input_poll();
    if (evt != EVT_NONE) {
        ui_handle_input(evt);
    }
    if (input_any_activity()) ui_notify_activity();
    ui_check_idle();

    display_lvgl_tick_and_handler();
    backlight_poll();
    delay(5);
}

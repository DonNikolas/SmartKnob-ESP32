#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "Arduino.h"
#include "test_platform.h"
#include "WiFi.h"
#include "../wifi_config.h"
#include "../ui.cpp"

static uint32_t nowMs = 100;
static int pins[50];
uint32_t millis() { return nowMs; }
int digitalRead(int pin) { return pins[pin]; }
void pinMode(int, int) {}
static lv_color_t buffer[240 * 80], pixels[240 * 240];
static int frame = 0;
static bool recording = false;

void flush(lv_disp_drv_t *d, const lv_area_t *area, lv_color_t *p) {
    for (int y = area->y1; y <= area->y2; ++y)
        for (int x = area->x1; x <= area->x2; ++x) pixels[y * 240 + x] = *p++;
    lv_disp_flush_ready(d);
}
void shot(const char *name) {
    lv_refr_now(NULL);
    FILE *f = fopen(name, "wb"); assert(f);
    fprintf(f, "P6\n240 240\n255\n");
    for (auto p : pixels) {
        uint32_t rgb = lv_color_to32(p);
        unsigned char b[] = {(unsigned char)(rgb >> 16), (unsigned char)(rgb >> 8), (unsigned char)rgb};
        fwrite(b, 1, 3, f);
    }
    fclose(f);
}
void advance(int ms) {
    for (int i = 0; i < ms; i += 5) {
        nowMs += 5; lv_tick_inc(5); network_time_poll(); ui_check_idle(); lv_timer_handler(); backlight_poll();
        if (recording && nowMs % 40 == 0) {
            char path[80]; snprintf(path, sizeof(path), "frames/%04d.ppm", frame++); shot(path);
        }
    }
}
void event(InputEvent e, int after = 240) { ui_handle_input(e); advance(after); }
void checkInput() {
    for (auto &p : pins) p = HIGH;
    input_init();
    // Bouncing direction input -> exactly one event after stable low.
    pins[PIN_ROT_RIGHT] = LOW; assert(input_poll() == EVT_NONE);
    nowMs += 5; pins[PIN_ROT_RIGHT] = HIGH; assert(input_poll() == EVT_NONE);
    nowMs += 5; pins[PIN_ROT_RIGHT] = LOW; assert(input_poll() == EVT_NONE);
    nowMs += DEBOUNCE_MS; assert(input_poll() == EVT_RIGHT);
    assert(input_poll() == EVT_NONE);
    nowMs += INPUT_REPEAT_DELAY_MS; assert(input_poll() == EVT_RIGHT);
    pins[PIN_ROT_RIGHT] = HIGH; input_poll(); nowMs += DEBOUNCE_MS; input_poll();
    pins[PIN_BTN_PRESS] = LOW; input_poll(); nowMs += DEBOUNCE_MS; input_poll();
    nowMs += LONGPRESS_MS; assert(input_poll() == EVT_PRESS_LONG);
    nowMs += 50; assert(input_poll() == EVT_NONE);
    pins[PIN_BTN_PRESS] = HIGH; assert(input_poll() == EVT_NONE);
    nowMs += DEBOUNCE_MS; assert(input_poll() == EVT_NONE);
    pins[PIN_BTN_PRESS] = LOW; input_poll(); nowMs += DEBOUNCE_MS; input_poll();
    nowMs += 80; pins[PIN_BTN_PRESS] = HIGH; input_poll();
    nowMs += DEBOUNCE_MS; assert(input_poll() == EVT_PRESS_SHORT);
    // Debounce timers also work across uint32_t millis rollover.
    nowMs = UINT32_MAX - 10;
    pins[PIN_ROT_LEFT] = LOW; input_poll(); nowMs += 30;
    assert(input_poll() == EVT_LEFT);
    for (auto &p : pins) p = HIGH;
    input_init();
    puts("PASS: debounce, repeat, short/long press, no release click, rollover");
}
int main() {
    checkInput();
    assert(settings_init());
    assert(backlight_init());
    network_time_init();
    nowMs = 100;
    lv_init();
    static lv_disp_draw_buf_t draw;
    static lv_disp_drv_t driver;
    lv_disp_draw_buf_init(&draw, buffer, NULL, 240*80);
    lv_disp_drv_init(&driver);
    driver.hor_res = driver.ver_res = 240; driver.flush_cb = flush; driver.draw_buf = &draw;
    assert(lv_disp_drv_register(&driver));
    ui_init(); advance(300);
    shot("home.ppm");
    recording = true;
    advance(600);
    event(EVT_RIGHT, 360); event(EVT_RIGHT, 360); event(EVT_LEFT, 360); event(EVT_LEFT, 600);
    event(EVT_PRESS_SHORT, 450); assert(state == KNOB_VIEW_VALUE); shot("music.ppm");
    event(EVT_LEFT, 70); event(EVT_LEFT, 70); event(EVT_LEFT, 70); event(EVT_LEFT, 70);
    advance(500);
    event(EVT_RIGHT, 80); event(EVT_RIGHT, 80); advance(400);
    event(EVT_PRESS_SHORT, 1000); assert(state == KNOB_VIEW_HOME); advance(300);
    recording = false;
    assert(apps[1].value == 65);
    // Cancel discards pending edits, even during an arc animation.
    event(EVT_PRESS_SHORT, 10); event(EVT_RIGHT, 10); assert(draftValue == 70);
    event(EVT_PRESS_LONG); assert(state == KNOB_VIEW_HOME && apps[1].value == 65);
    // Rapid carousel retargeting and direction changes retain all steps.
    for (int i = 0; i < 101; ++i) event(EVT_RIGHT, 5);
    for (int i = 0; i < 100; ++i) event(EVT_LEFT, 5);
    advance(300); assert(selected == 2 && carouselPosition == carouselTarget);
    event(EVT_PRESS_SHORT); assert(state == KNOB_VIEW_VALUE);
    for (int i = 0; i < 60; ++i) event(EVT_RIGHT, 5);
    advance(300); assert(draftValue == 100 && displayedArc == 1000);
    shot("limit.ppm");
    for (int i = 0; i < 60; ++i) event(EVT_LEFT, 5);
    advance(300); assert(draftValue == 0 && displayedArc == 0);
    event(EVT_PRESS_LONG); assert(apps[2].value == 60);
    event(EVT_RIGHT); event(EVT_PRESS_SHORT); assert(draftValue == 220);
    event(EVT_RIGHT); assert(draftValue == 225);
    assert(strcmp(lv_label_get_text(valueText), "22.5") == 0); shot("temperature.ppm");
    event(EVT_PRESS_SHORT, 100); shot("confirmed.ppm"); advance(900);
    event(EVT_RIGHT); event(EVT_RIGHT); event(EVT_PRESS_SHORT); assert(state == KNOB_VIEW_MENU);
    shot("menu.ppm");
    event(EVT_RIGHT); event(EVT_RIGHT); event(EVT_RIGHT); event(EVT_PRESS_SHORT);
    assert(state == KNOB_VIEW_HOME); // Previously broken "Zurück" item.
    event(EVT_PRESS_SHORT); event(EVT_PRESS_SHORT); assert(state == KNOB_VIEW_STATUS);
    advance(2300); assert(state == KNOB_VIEW_MENU);
    event(EVT_PRESS_LONG); advance(IDLE_TIMEOUT_MS + 40); assert(state == KNOB_VIEW_STANDBY);
    shot("standby.ppm");
    int selectionBeforeWake = selected;
    event(EVT_LEFT); assert(state == KNOB_VIEW_HOME && selected == selectionBeforeWake);
    // Persistent view lifecycle: allocator usage must settle after repeated edits.
    selected = 1; carouselTarget = 1000; showView(KNOB_VIEW_HOME); advance(300);
    lv_mem_monitor_t before, after;
    lv_mem_monitor(&before);
    for (int i = 0; i < 300; ++i) {
        event(EVT_PRESS_SHORT, 5); event(EVT_RIGHT, 5); event(EVT_PRESS_LONG, 5);
    }
    advance(300); lv_mem_monitor(&after);
    assert(after.free_size == before.free_size);
    assert(lv_mem_test() == LV_RES_OK);
    // Auto-return and idle checks must survive timer rollover too.
    nowMs = UINT32_MAX - 300; ui_notify_activity();
    event(EVT_PRESS_SHORT, 10); event(EVT_PRESS_SHORT, 10);
    assert(state == KNOB_VIEW_STATUS); advance(900); assert(state == KNOB_VIEW_HOME);
    nowMs = UINT32_MAX - 100; ui_notify_activity();
    advance(IDLE_TIMEOUT_MS + 30); assert(state == KNOB_VIEW_STANDBY);
    printf("PASS: LVGL 8.3.11 rendering, rapid turns, exact steps, min/max, cancel/commit, menu/back, standby/wake, timers\n");
    printf("PASS: 300 edit/cancel cycles; free LVGL heap %lu -> %lu bytes\n",
           (unsigned long)before.free_size, (unsigned long)after.free_size);
    printf("Rendered %d animation frames\n", frame);

    // Full new services run here; only the ESP32 interfaces are simulated.
    hostNvsOpen = false; assert(!settings_init());
    assert(settings_load("absent", 80, 10, 100, 5) == 80);
    assert(!settings_save("backlight", 55));
    hostNvsOpen = true; assert(settings_init());
    hostNvs["backlight"] = -200;
    assert(settings_load("backlight", 80, 10, 100, 5) == 10);
    hostNvs["backlight"] = 99999;
    assert(settings_load("backlight", 80, 10, 100, 5) == 100);
    hostNvs["temperature"] = 227;
    assert(settings_load("temperature", 220, 160, 300, 5) == 225);

    selected = kDisplayApp; carouselTarget = 4000;
    showView(KNOB_VIEW_HOME); backlight_set_percent(80, false);
    event(EVT_PRESS_SHORT); int writes = hostWrites;
    for (int i = 0; i < 50; ++i) event(EVT_LEFT, 5);
    advance(400);
    assert(draftValue == 10 && backlight_target_percent() == 10 && hostDuty == 102);
    assert(hostWrites == writes); // Preview must not write to flash.
    shot("brightness_10.ppm");
    event(EVT_PRESS_LONG, 400);
    assert(apps[kDisplayApp].value == 80 && hostDuty == 818);
    event(EVT_PRESS_SHORT);
    for (int i = 0; i < 50; ++i) event(EVT_RIGHT, 5);
    advance(400); assert(draftValue == 100 && hostDuty == 1023);
    event(EVT_PRESS_SHORT, 900);
    assert(hostNvs["backlight"] == 100 && apps[kDisplayApp].value == 100);
    writes = hostWrites;
    event(EVT_PRESS_SHORT); event(EVT_PRESS_SHORT, 900);
    assert(hostWrites == writes); // Unchanged confirmation is not rewritten.
    event(EVT_PRESS_SHORT); event(EVT_LEFT);
    hostNvsWrite = false;
    event(EVT_PRESS_SHORT, 400);
    assert(state == KNOB_VIEW_STATUS && apps[kDisplayApp].value == 100 && hostDuty == 1023);
    assert(strcmp(lv_label_get_text(statusTitle), "Speicherfehler") == 0);
    hostNvsWrite = true; advance(2000);
    assert(settings_init()); // Reopen namespace: values survive restart of service.
    assert(settings_load("backlight", 80, 10, 100, 5) == 100);
    assert(settings_load("volume", 75, 0, 100, 5) == apps[1].value);

    // Native time API: no fake uptime on screen; handle missing WLAN/NTP.
    assert(network_time_configured() == (KNOB_WIFI_SSID[0] != '\0'));
    event(EVT_PRESS_LONG, 400);
    assert(state == KNOB_VIEW_STANDBY && hostDuty == 102);
    assert(strcmp(lv_label_get_text(standbyTime), "--:--") == 0);
    shot("standby_waiting.ppm");
    if (network_time_configured()) {
        nowMs = UINT32_MAX - 100; network_time_init();
        int attempts = hostReconnects;
        advance(KNOB_WIFI_RETRY_MS + 10); assert(hostReconnects > attempts);
        hostWifiState = WL_CONNECTED;
        advance(1000); assert(hostNtpStarts == 1);
        assert(strcmp(lv_label_get_text(standbyTime), "--:--") == 0);
        hostEpoch = 1789381680; // 2026-09-14 10:28 UTC -> 12:28 CEST.
        advance(1000);
        assert(strcmp(lv_label_get_text(standbyTime), "12:28") == 0);
        assert(strcmp(lv_label_get_text(standbyDate), "Mo, 14.09.2026") == 0);
        shot("standby_clock.ppm");
        hostWifiState = WL_DISCONNECTED; hostEpoch += 60; advance(1000);
        assert(strcmp(lv_label_get_text(standbyTime), "12:29") == 0);
        assert(strcmp(lv_label_get_text(standbyStatus), "WLAN OFFLINE") == 0);
        shot("standby_offline.ppm");
        hostWifiState = WL_CONNECTED; advance(1000);
        assert(hostNtpStarts == 1); // Do not restart SNTP on every reconnect/frame.
        hostEpoch = 1768474800; // 2026-01-15 11:00 UTC -> 12:00 CET.
        advance(1000); assert(strcmp(lv_label_get_text(standbyTime), "12:00") == 0);
        struct tm utc = {};
        utc.tm_year = 126; utc.tm_mon = 2; utc.tm_mday = 29; utc.tm_min = 59;
        hostEpoch = timegm(&utc);
        struct tm local;
        assert(network_time_local(&local) && local.tm_hour == 1 && local.tm_min == 59);
        hostEpoch += 60;
        assert(network_time_local(&local) && local.tm_hour == 3 && local.tm_min == 0);
        utc.tm_mon = 9; utc.tm_mday = 25;
        hostEpoch = timegm(&utc);
        assert(network_time_local(&local) && local.tm_hour == 2 && local.tm_min == 59);
        hostEpoch += 60;
        assert(network_time_local(&local) && local.tm_hour == 2 && local.tm_min == 0);
    } else {
        assert(hostReconnects == 0 && hostNtpStarts == 0);
        assert(strcmp(lv_label_get_text(standbyStatus), "WLAN EINTRAGEN") == 0);
    }
    event(EVT_RIGHT, 400); assert(state == KNOB_VIEW_HOME && hostDuty == 1023);
    nowMs = UINT32_MAX - 100;
    backlight_set_percent(-10); advance(400); assert(hostDuty == 102);
    backlight_set_percent(1000); advance(400); assert(hostDuty == 1023);
    const int restored[] = {30, 45, 20, 235, 55};
    for (int i = 0; i < APP_COUNT - 1; ++i) {
        assert(settings_save(settingKeys[i], restored[i]));
        apps[i].value = apps[i].min;
    }
    assert(settings_init());
    lv_obj_t *oldRoot = root;
    ui_init(); // Restart UI using persisted values, not the old app array.
    lv_obj_del(oldRoot);
    for (int i = 0; i < APP_COUNT - 1; ++i) assert(apps[i].value == restored[i]);
    assert(backlight_target_percent() == 55 && hostDuty == 563);
    hostPwmAttach = false; assert(!backlight_init());
    puts("PASS: NVS bounds/errors/write suppression/reopen, live PWM 10..100, cancel/failure rollback, standby fade, clock/reconnect/timezones");
}

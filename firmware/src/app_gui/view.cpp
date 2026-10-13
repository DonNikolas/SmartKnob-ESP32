// App renderer adapted with ChatGPT assistance from the Arduino UI prototype.
// Core 0 owns LVGL, SPI3, NeoPixel and backlight. Core 1 owns all app/motor state.
#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <Adafruit_NeoPixel.h>
#include <lvgl.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <math.h>
#include <string.h>
#include "view.h"
#include "knob_fonts.h"

#if !CONFIG_IDF_TARGET_ESP32S3 || ARDUINO_RUNNING_CORE != 1
#error "This GUI requires ESP32-S3 with the Arduino loop on core 1."
#endif
#if LVGL_VERSION_MAJOR != 8 || LVGL_VERSION_MINOR != 3
#error "Use LVGL 8.3.x with the existing include/lv_conf.h."
#endif
namespace KnobView {
namespace {
using namespace KnobApp;
constexpr uint32_t UI_BG = 0x080C10, UI_PANEL = 0x141C23, UI_TRACK = 0x27323B;
constexpr uint32_t UI_TEXT = 0xEDF4EF, UI_MUTED = 0x84949F;
constexpr float kKnobPi = 3.14159265359f;
constexpr uint32_t DISPLAY_SPI_HZ = 20000000UL;
// Arduino S3 HSPI = physical SPI3. Encoder uses SPI2_HOST separately.
Arduino_ESP32SPI bus(18, 42, 3, 8, GFX_NOT_DEFINED, HSPI);
Arduino_GC9A01 display(&bus, 10, 0, true);
Adafruit_NeoPixel ring(LED_COUNT, 11, NEO_GRB + NEO_KHZ800);
QueueHandle_t states = nullptr, resultQueue = nullptr;
lv_color_t pixels[240 * 20];
lv_disp_draw_buf_t drawBuffer;
lv_disp_drv_t displayDriver;
lv_obj_t *views[9], *cards[APP_COUNT], *homeTitle, *homeValue;
lv_obj_t *valueTitle, *valueText, *valueUnit, *valueIcon, *valueDots[36];
lv_obj_t *menuFocus, *menuRows[4], *noticeTitle, *noticeText, *noticeIcon;
lv_obj_t *sensorTemperature, *sensorHumidity, *sensorPressure, *sensorStatus, *audioStatus;
lv_obj_t *bootText, *stopTitle, *stopReason, *faultReason;
enum Icon { KNOB_ICON_TIMER, KNOB_ICON_MUSIC, KNOB_ICON_SUN, KNOB_ICON_THERMO,
 KNOB_ICON_DISPLAY, KNOB_ICON_MORE, KNOB_ICON_CHECK, KNOB_ICON_CROSS, KNOB_ICON_INFO };
Icon homeIcons[APP_COUNT] = {KNOB_ICON_TIMER, KNOB_ICON_MUSIC, KNOB_ICON_SUN,
                    KNOB_ICON_THERMO, KNOB_ICON_DISPLAY, KNOB_ICON_THERMO, KNOB_ICON_MORE};
Icon editorIcon = KNOB_ICON_TIMER, messageIcon = KNOB_ICON_CHECK;
const char* menuNames[] = {"Info", "Status demo", "Standby", "Back"};
lv_color_t color(uint32_t rgb) { return lv_color_hex(rgb); }
int viewIndex(Screen s) { return static_cast<int>(s); }
lv_obj_t* box(lv_obj_t* parent, int w, int h) {
    lv_obj_t* obj = lv_obj_create(parent); lv_obj_remove_style_all(obj);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(obj, w, h); return obj;
}
lv_obj_t* label(lv_obj_t* parent, const char* text, const lv_font_t* font,
                int y, uint32_t tint = UI_TEXT, int width = 200) {
    lv_obj_t* obj = lv_label_create(parent); lv_label_set_text(obj, text);
    lv_obj_set_width(obj, width); lv_obj_set_style_text_font(obj, font, 0);
    lv_obj_set_style_text_color(obj, color(tint), 0);
    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(obj, LV_ALIGN_TOP_MID, 0, y); return obj;
}
void iconDraw(lv_event_t *e) {
    lv_obj_t *o = lv_event_get_target(e);
    lv_draw_ctx_t *ctx = lv_event_get_draw_ctx(e);
    const Icon icon = *(Icon *)lv_event_get_user_data(e);
    lv_area_t area;
    lv_obj_get_coords(o, &area);
    const float s = lv_obj_get_width(o) / 40.0f;
    lv_draw_line_dsc_t d;
    lv_draw_line_dsc_init(&d);
    d.color = lv_obj_get_style_text_color(o, LV_PART_MAIN);
    d.width = s >= 1.0f ? 4 : 3;
    d.round_start = d.round_end = 1;
    auto point = [&](float x, float y) -> lv_point_t {
        return {(lv_coord_t)(area.x1 + lroundf(x * s)),
                (lv_coord_t)(area.y1 + lroundf(y * s))};
    };
    auto line = [&](float x1, float y1, float x2, float y2) {
        lv_point_t a = point(x1, y1), b = point(x2, y2);
        lv_draw_line(ctx, &d, &a, &b);
    };
    auto circle = [&](float x, float y, float radius, bool fill) {
        lv_draw_arc_dsc_t a;
        lv_draw_arc_dsc_init(&a);
        a.color = d.color;
        a.width = fill ? (lv_coord_t)(radius * s + 1) : d.width;
        lv_point_t c = point(x, y);
        lv_draw_arc(ctx, &a, &c, (uint16_t)lroundf(radius * s), 0, 360);
    };
    switch (icon) {
        case KNOB_ICON_MUSIC:
            line(16, 29, 16, 11); line(16, 11, 31, 7); line(31, 7, 31, 25);
            circle(11, 29, 5, true); circle(26, 25, 5, true); break;
        case KNOB_ICON_SUN:
            circle(20, 20, 8, false);
            for (int i = 0; i < 9; ++i) {
                float a = i * kKnobPi / 4;
                line(20 + cosf(a)*13, 20 + sinf(a)*13,
                     20 + cosf(a)*17, 20 + sinf(a)*17);
            }
            break;
        case KNOB_ICON_THERMO:
            line(16, 25, 16, 9); line(24, 25, 24, 9); line(16, 8, 24, 8);
            circle(20, 29, 7, false); line(20, 16, 20, 30);
            line(28, 13, 32, 13); line(28, 19, 31, 19); break;
        case KNOB_ICON_DISPLAY:
            line(6, 9, 34, 9); line(34, 9, 34, 28); line(34, 28, 6, 28);
            line(6, 28, 6, 9); line(20, 28, 20, 34); line(13, 34, 27, 34); break;
        case KNOB_ICON_TIMER:
            circle(20, 23, 13, false); line(16, 4, 24, 4); line(20, 4, 20, 8);
            line(20, 23, 20, 15); line(20, 23, 26, 26); break;
        case KNOB_ICON_MORE:
            circle(9, 20, 3, true); circle(20, 20, 3, true); circle(31, 20, 3, true); break;
        case KNOB_ICON_CHECK:
            line(9, 21, 17, 29); line(17, 29, 32, 12); break;
        case KNOB_ICON_CROSS:
            line(11, 11, 29, 29); line(29, 11, 11, 29); break;
        case KNOB_ICON_INFO:
            circle(20, 10, 2, true); line(20, 18, 20, 31); break;
    }
}

lv_obj_t *makeIcon(lv_obj_t *parent, Icon *icon, uint32_t rgb, int size) {
    lv_obj_t *o = box(parent, size, size);
    lv_obj_set_style_text_color(o, color(rgb), 0);
    lv_obj_add_event_cb(o, iconDraw, LV_EVENT_DRAW_MAIN, icon);
    lv_obj_center(o);
    return o;
}


void flush(lv_disp_drv_t* driver, const lv_area_t* area, lv_color_t* colors) {
    display.draw16bitRGBBitmap(area->x1, area->y1, reinterpret_cast<uint16_t*>(colors),
                              area->x2 - area->x1 + 1, area->y2 - area->y1 + 1);
    lv_disp_flush_ready(driver);
}
void numberText(char* text, size_t size, int value, const App& app) {
    if (app.divisor == 10) snprintf(text, size, "%d.%d", value / 10, value % 10);
    else snprintf(text, size, "%d", value);
}
int fillCount(const State& s) {
    const App& a = APPS[s.selected];
    if (!a.step) return 0;
    return (s.draft - a.low) * LED_COUNT / (a.high - a.low);
}
void createViews() {
    lv_obj_t* root = lv_scr_act();
    lv_obj_set_style_bg_color(root, color(UI_BG), 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(root, 0, 0);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    for (int i = 0; i < 9; ++i) {
        views[i] = box(root, 240, 240);
        lv_obj_add_flag(views[i], LV_OBJ_FLAG_HIDDEN);
    }
    auto v = views[viewIndex(Screen::Boot)];
    label(v, "Release knob", &knob_font_24, 89);
    bootText = label(v, "Starting...", &knob_font_16, 132);
    v = views[viewIndex(Screen::Home)];
    for (int i = 0; i < APP_COUNT; ++i) {
        cards[i] = box(v, 92, 92);
        lv_obj_set_style_radius(cards[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(cards[i], LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(cards[i], 2, 0);
        makeIcon(cards[i], &homeIcons[i], APPS[i].colour, 46);
    }
    homeTitle = label(v, "Timer", &knob_font_24, 156);
    homeValue = label(v, "15 min", &knob_font_16, 186);
    v = views[viewIndex(Screen::Sensors)];
    label(v, "SENSORS", &knob_font_16, 23, APPS[SENSOR_APP].colour);
    sensorTemperature = label(v, "--.- °C", &knob_font_24, 53);
    sensorHumidity = label(v, "--.- %RH", &knob_font_24, 88);
    sensorPressure = label(v, "----.- hPa", &knob_font_24, 123);
    sensorStatus = label(v, "Starting sensor", &knob_font_14, 161, UI_MUTED);
    audioStatus = label(v, "Starting microphone", &knob_font_14, 182, UI_MUTED);
    v = views[viewIndex(Screen::Value)];
    for (int i = 0; i < LED_COUNT; ++i) {
        const float angle = (90 - i * 360.0f / LED_COUNT) * kKnobPi / 180;
        valueDots[i] = box(v, 5, 5);
        lv_obj_set_pos(valueDots[i], 120 + lroundf(113 * cosf(angle)) - 2,
                                      120 + lroundf(113 * sinf(angle)) - 2);
        lv_obj_set_style_radius(valueDots[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(valueDots[i], LV_OPA_COVER, 0);
    }
    valueTitle = label(v, "DURATION", &knob_font_14, 35, UI_MUTED, 160);
    valueIcon = makeIcon(v, &editorIcon, APPS[0].colour, 32);
    lv_obj_align(valueIcon, LV_ALIGN_TOP_MID, 0, 59);
    valueText = label(v, "15", &knob_font_52, 97, UI_TEXT, 180);
    valueUnit = label(v, "min", &knob_font_18, 153, UI_MUTED);
    v = views[viewIndex(Screen::Menu)];
    label(v, "MORE", &knob_font_16, 35, UI_MUTED);
    menuFocus = box(v, 168, 29); lv_obj_set_pos(menuFocus, 36, 64);
    lv_obj_set_style_radius(menuFocus, 9, 0);
    lv_obj_set_style_bg_color(menuFocus, color(0x253A36), 0);
    lv_obj_set_style_bg_opa(menuFocus, LV_OPA_COVER, 0);
    for (int i = 0; i < 4; ++i) menuRows[i] = label(v, menuNames[i], &knob_font_16, 69 + 31 * i);
    v = views[viewIndex(Screen::Status)];
    noticeIcon = makeIcon(v, &messageIcon, 0xAEF582, 54);
    lv_obj_align(noticeIcon, LV_ALIGN_TOP_MID, 0, 51);
    noticeTitle = label(v, "", &knob_font_24, 131);
    noticeText = label(v, "", &knob_font_14, 168, UI_MUTED, 188);
    v = views[viewIndex(Screen::Standby)];
    label(v, "Standby", &knob_font_24, 100);
    v = views[viewIndex(Screen::Stopped)];
    stopTitle = label(v, "Stopped", &knob_font_24, 53);
    stopReason = label(v, "Motor off", &knob_font_14, 99, UI_MUTED, 180);
    label(v, "Reset / r to restart", &knob_font_14, 178, UI_MUTED, 176);
    v = views[viewIndex(Screen::Fault)];
    label(v, "Motor stopped", &knob_font_24, 48, 0xFF8B86);
    faultReason = label(v, "Check hardware", &knob_font_14, 94, UI_TEXT, 178);
    label(v, "Reset / r after checking", &knob_font_14, 177, UI_MUTED, 180);
}
// Follow the measured knob position, including its physical spring return.
// No extra spring integration or queued animation creates visual lag.
float carouselPosition = 0;
float carouselDistance(float d) {
    while (d > APP_COUNT / 2.0f) d -= APP_COUNT;
    while (d < -APP_COUNT / 2.0f) d += APP_COUNT;
    return d;
}
bool updateCarousel(const State& s, bool reset) {
    float target = s.selected + s.homeOffset;
    // About half a pixel per step; avoid redraws for subpixel sensor noise.
    if (!reset && fabsf(carouselDistance(target - carouselPosition)) < 0.006f) return false;
    carouselPosition = target;
    return true;
}
struct CardCache { int x = -999, y = -999, size = -1, shade = -1, iconSize = -1; };
CardCache cardCache[APP_COUNT];
int carouselFront = -1;
void home(const State& s, bool textChanged = true) {
    for (int i = 0; i < APP_COUNT; ++i) {
        // Increasing selection is shown on the LEFT, matching CCW knob motion.
        float slot = -carouselDistance(i - carouselPosition);
        if (fabsf(slot) > 1.9f) { lv_obj_add_flag(cards[i], LV_OBJ_FLAG_HIDDEN); continue; }
        lv_obj_clear_flag(cards[i], LV_OBJ_FLAG_HIDDEN);
        float focus = 1.0f - fminf(fabsf(slot), 1.0f);
        int size = lroundf(46 + 46 * focus);
        int x = lroundf(120 + slot * 83) - size / 2;
        int y = lroundf(118 - 15 * focus) - size / 2;
        int shade = lroundf(focus * 16); // Small colour changes do not need new styles.
        int iconSize = lroundf(28 + 18 * focus);
        CardCache& c = cardCache[i];
        auto icon = lv_obj_get_child(cards[i], 0);
        if (size != c.size) lv_obj_set_size(cards[i], size, size);
        if (x != c.x || y != c.y) lv_obj_set_pos(cards[i], x, y);
        if (shade != c.shade) {
            lv_obj_set_style_bg_color(cards[i], lv_color_mix(color(APPS[i].colour), color(UI_PANEL), shade * 24 / 16), 0);
            lv_obj_set_style_border_color(cards[i], lv_color_mix(color(APPS[i].colour), color(UI_TRACK), shade * 255 / 16), 0);
            lv_obj_set_style_text_color(icon, lv_color_mix(color(APPS[i].colour), color(UI_MUTED), shade * 255 / 16), 0);
        }
        if (iconSize != c.iconSize || size != c.size) {
            lv_obj_set_size(icon, iconSize, iconSize); lv_obj_center(icon);
        }
        c.x = x; c.y = y; c.size = size; c.shade = shade; c.iconSize = iconSize;
    }
    int front = wrap(lroundf(carouselPosition), APP_COUNT);
    if (front != carouselFront) { lv_obj_move_foreground(cards[front]); carouselFront = front; }
    if (!textChanged) return;
    lv_label_set_text(homeTitle, APPS[s.selected].name);
    char n[16], text[40];
    if (s.selected == MORE_APP) snprintf(text, sizeof(text), "%s", "");
    else if (s.selected == SENSOR_APP) snprintf(text, sizeof(text), "%s", "");
    else {
        numberText(n, sizeof(n), s.values[s.selected], APPS[s.selected]);
        snprintf(text, sizeof(text), "%s %s", n, APPS[s.selected].unit);
    }
    lv_label_set_text(homeValue, text);
    lv_obj_set_style_text_color(homeValue, color(APPS[s.selected].colour), 0);
}
void render(const State& s, const State& old, bool first) {
    bool changed = first || s.screen != old.screen;
    if (changed) {
        for (auto* view : views) lv_obj_add_flag(view, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(views[viewIndex(s.screen)], LV_OBJ_FLAG_HIDDEN);
    }
    switch (s.screen) {
    case Screen::Home: {
        bool moved = updateCarousel(s, changed);
        bool textChanged = changed || s.selected != old.selected || memcmp(s.values, old.values, sizeof(s.values));
        if (moved || textChanged) home(s, textChanged);
        break;
    }
    case Screen::Sensors:
        if (changed || s.environment.sequence != old.environment.sequence ||
            s.environment.valid != old.environment.valid || s.environment.status != old.environment.status) {
            char t[40];
            if (s.environment.valid) snprintf(t, sizeof(t), "%.1f °C", s.environment.temperature);
            else snprintf(t, sizeof(t), "--.- °C");
            lv_label_set_text(sensorTemperature, t);
            if (s.environment.valid) snprintf(t, sizeof(t), "%.1f %%RH", s.environment.humidity);
            else snprintf(t, sizeof(t), "--.- %%RH");
            lv_label_set_text(sensorHumidity, t);
            if (s.environment.valid) snprintf(t, sizeof(t), "%.1f hPa", s.environment.pressure);
            else snprintf(t, sizeof(t), "----.- hPa");
            lv_label_set_text(sensorPressure, t);
            lv_label_set_text(sensorStatus, s.environment.valid ? "" : s.environment.status);
        }
        if (changed || s.audio.status != old.audio.status) lv_label_set_text(audioStatus, strcmp(s.audio.status, "Clap: ready") == 0 ? "" : s.audio.status);
        break;
    case Screen::Value: {
        const App& a = APPS[s.selected];
        bool entered = changed || s.selected != old.selected;
        if (entered) {
            editorIcon = homeIcons[s.selected]; lv_obj_invalidate(valueIcon);
            lv_obj_set_style_text_color(valueIcon, color(a.colour), 0);
            lv_label_set_text(valueTitle, a.caption); lv_label_set_text(valueUnit, a.unit);
        }
        if (entered || s.draft != old.draft) {
            char n[16]; numberText(n, sizeof(n), s.draft, a); lv_label_set_text(valueText, n);
            int count = fillCount(s), was = entered ? -1 : fillCount(old);
            for (int i = 0; i < LED_COUNT; ++i)
                if (entered || (i < count) != (i < was))
                    lv_obj_set_style_bg_color(valueDots[i], color(i < count ? a.colour : UI_TRACK), 0);
        }
        break;
    }
    case Screen::Menu:
        if (changed || s.menu != old.menu) {
            lv_obj_set_y(menuFocus, 64 + s.menu * 31);
            for (int i = 0; i < 4; ++i) lv_obj_set_style_text_color(menuRows[i], color(i == s.menu ? UI_TEXT : UI_MUTED), 0);
        }
        break;
    case Screen::Status:
        if (changed || s.notice != old.notice) {
            messageIcon = s.notice == Notice::Applied ? KNOB_ICON_CHECK : s.notice == Notice::Info ? KNOB_ICON_INFO : KNOB_ICON_CROSS;
            lv_obj_invalidate(noticeIcon);
            lv_obj_set_style_text_color(noticeIcon, color(s.notice == Notice::Demo ? 0xFF8B86 : APPS[s.selected].colour), 0);
            lv_label_set_text(noticeTitle, s.notice == Notice::Applied ? "" : s.notice == Notice::Info ? "Smart Knob" : "Status demo");
            lv_label_set_text(noticeText, s.notice == Notice::Applied ? "" :
                s.notice == Notice::Info ? (s.weakField ? "Local apps / weak field" : "ESP32-S3 / local apps") : "Example message only");
        }
        break;
    case Screen::Boot:
        if (changed || s.zeroing != old.zeroing || s.zeroSamples != old.zeroSamples) {
            char text[40];
            if (s.zeroing) snprintf(text, sizeof(text), "Zeroing: %u / 32", s.zeroSamples);
            else snprintf(text, sizeof(text), "%s", s.zeroSamples ? "Preparing controls..." : "Starting...");
            lv_label_set_text(bootText, text);
        }
        break;
    case Screen::Stopped: if (changed) lv_label_set_text(stopReason, s.reason); break;
    case Screen::Fault: if (changed) lv_label_set_text(faultReason, s.reason); break;
    default: break;
    }
}
uint32_t ledBlend(uint32_t a, uint32_t b, float amount) {
    amount = fmaxf(0, fminf(1, amount));
    uint32_t result = 0;
    for (unsigned shift = 0; shift <= 16; shift += 8) {
        float av = (a >> shift) & 255, bv = (b >> shift) & 255;
        result |= uint32_t(lroundf(av + (bv - av) * amount)) << shift;
    }
    return result;
}
void drawHomeGlow(const State& s, bool flash) {
    // Same continuous app coordinate as the carousel, including spring return.
    float phase = fmodf(s.selected + s.homeOffset + APP_COUNT, float(APP_COUNT));
    int base = int(floorf(phase));
    // Position-based colour easing: exact app colour at each detent centre,
    // continuously blended in either direction, without a timed animation lag.
    float fraction = phase - base;
    float blend = fraction * fraction * (3.0f - 2.0f * fraction);
    uint32_t tint = flash ? 0xFFFFFF : ledBlend(APPS[base].colour,
        APPS[wrap(base + 1, APP_COUNT)].colour, blend);
    float centre = phase * LED_COUNT / APP_COUNT;
    constexpr float radius = 3.0f; // A soft arc spanning about six LEDs.
    for (int i = 0; i < LED_COUNT; ++i) {
        float distance = fabsf(i - centre);
        distance = fminf(distance, LED_COUNT - distance);
        float intensity = distance < radius
            ? 0.5f + 0.5f * cosf(kKnobPi * distance / radius) : 0;
        ring.setPixelColor(ledIndex(i), ledBlend(0, tint, intensity));
    }
}
void drawRing(const State& s, bool flash, bool clapFlash = false) {
    ring.clear();
    uint32_t tint = APPS[s.selected].colour;
    auto set = [&](int offset, uint32_t rgb) { ring.setPixelColor(ledIndex(offset), rgb); };
    if (s.screen == Screen::Home) drawHomeGlow(s, flash);
    else if (s.screen == Screen::Value) {
        const App& a = APPS[s.selected];
        float filled = float(s.draft - a.low) * LED_COUNT / (a.high - a.low);
        // Fractional endpoint: small value steps fade an LED instead of jumping.
        for (int i = 0; i < LED_COUNT; ++i)
            set(i, ledBlend(0, tint, fmaxf(0, fminf(1, filled - i))));
        if (flash) set(filled > 0 ? int(ceilf(filled)) - 1 : 0, 0xFFFFFF);
    } else if (s.screen == Screen::Menu) set(s.menu * 9, flash ? 0xFFFFFF : tint);
    else if (s.screen == Screen::Status) set(s.selected * LED_COUNT / APP_COUNT, s.notice == Notice::Demo ? 0xFF5040 : 0x55D98B);
    else if (s.screen == Screen::Boot) set(0, 0xFFB850);
    else if (s.screen == Screen::Stopped || s.screen == Screen::Fault) set(0, 0xFF3020);
    if (s.screen == Screen::Sensors) drawHomeGlow(s, flash);
    // Fault/stop indication takes precedence. A clap never changes app/motor state.
    if (clapFlash && s.screen != Screen::Fault && s.screen != Screen::Stopped)
        for (int i = 0; i < LED_COUNT; ++i) set(i, 0xFFFFFF);
    ring.show();
}
int brightness(const State& s) {
    return s.screen == Screen::Standby ? 10 :
           s.screen == Screen::Value && s.selected == 4 ? s.draft : s.values[4];
}
void task(void*) {
    ring.begin(); ring.setBrightness(20); ring.clear(); ring.show();
    bool ok = display.begin(DISPLAY_SPI_HZ);
    if (!ok) { xQueueSend(resultQueue, &ok, 0); vTaskDelete(nullptr); return; }
    lv_init(); lv_disp_draw_buf_init(&drawBuffer, pixels, nullptr, 240 * 20);
    lv_disp_drv_init(&displayDriver);
    displayDriver.hor_res = displayDriver.ver_res = 240;
    displayDriver.draw_buf = &drawBuffer; displayDriver.flush_cb = flush;
    if (!lv_disp_drv_register(&displayDriver)) { ok = false; xQueueSend(resultQueue, &ok, 0); vTaskDelete(nullptr); return; }
    createViews();
    State s, old; render(s, old, true); drawRing(s, false); lv_refr_now(nullptr);
    ledcSetup(0, 5000, 10); ledcAttachPin(9, 0); ledcWrite(0, 818);
    KnobBackground::begin();
    xQueueSend(resultQueue, &ok, 0);
    KnobBackground::Environment environment; KnobBackground::Audio audio;
    uint32_t pairsSeen = 0, clapAt = 0; bool clapActive = false, lastClapFlash = false;
    bool first = true, lastFlash = false, flashing = false;
    uint32_t previousTick = millis(), clickCount = 0, flashAt = 0;
    for (;;) {
        State next; if (xQueueReceive(states, &next, 0) == pdTRUE) s = next;
        uint32_t now = millis();
        KnobBackground::read(environment, audio);
        s.environment = environment; s.audio = audio;
        if (s.environment.valid && now - s.environment.measuredAt > 6000) {
            s.environment.valid = false; s.environment.status = "Sensor data stale";
        }
        if (audio.pairs != pairsSeen) {
            pairsSeen = audio.pairs; clapAt = now;
            clapActive = now - audio.lastPairAt < 500;
        }
        bool clapFlash = clapActive && now - clapAt < 200;
        if (!clapFlash) clapActive = false;
        if (s.clicks != clickCount) { clickCount = s.clicks; flashAt = now; flashing = true; }
        bool flash = flashing && now - flashAt < 150; if (!flash) flashing = false;
        if (first || s.screen != old.screen || s.selected != old.selected || s.menu != old.menu ||
            s.draft != old.draft || s.notice != old.notice || flash != lastFlash || clapFlash != lastClapFlash ||
            (s.screen == Screen::Home && fabsf(s.homeOffset - old.homeOffset) > 0.0001f)) drawRing(s, flash, clapFlash);
        if (first || brightness(s) != brightness(old)) ledcWrite(0, brightness(s) * 1023UL / 100);
        render(s, old, first);
        old = s; first = false; lastFlash = flash; lastClapFlash = clapFlash;
        lv_tick_inc(now - previousTick); previousTick = now; lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
} // namespace
bool begin() {
    states = xQueueCreate(1, sizeof(KnobApp::State)); resultQueue = xQueueCreate(1, sizeof(bool));
    if (!states || !resultQueue) return false;
    if (xTaskCreatePinnedToCore(task, "app_gui", 12288, nullptr, 1, nullptr, 0) != pdPASS) return false;
    bool ok = false;
    return xQueueReceive(resultQueue, &ok, pdMS_TO_TICKS(15000)) == pdTRUE && ok;
}
void publish(const KnobApp::State& state) { if (states) xQueueOverwrite(states, &state); }
} // namespace KnobView

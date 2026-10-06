#include "ui.h"
#include "config.h"
#include "knob_fonts.h"
#include "settings.h"
#include "network_time.h"
#include "backlight.h"
#include <Arduino.h>
#include <lvgl.h>
#include <math.h>
#include <stdio.h>

#if LVGL_VERSION_MAJOR != 8 || LVGL_VERSION_MINOR != 3
#error "Dieser Sketch benötigt LVGL 8.3.x (getestet: 8.3.11)."
#endif
#if LV_COLOR_DEPTH != 16 || LV_COLOR_16_SWAP != 0
#error "Bitte die mitgelieferte lv_conf.h verwenden: RGB565, LV_COLOR_16_SWAP=0."
#endif

namespace {
constexpr uint32_t BG = 0x080C10, PANEL = 0x141C23, TRACK = 0x27323B;
constexpr uint32_t WHITE = 0xEDF4EF, MUTED = 0x84949F;
// Prefix internal names: Arduino defines PI/DISPLAY, ESP-ROM defines STATUS.
constexpr float kKnobPi = 3.14159265359f;
enum Icon { KNOB_ICON_TIMER, KNOB_ICON_MUSIC, KNOB_ICON_SUN, KNOB_ICON_THERMO, KNOB_ICON_DISPLAY, KNOB_ICON_MORE, KNOB_ICON_CHECK, KNOB_ICON_CROSS, KNOB_ICON_INFO };
struct App {
    const char *name, *caption, *unit;
    Icon icon;
    uint32_t color;
    int min, max, step, value, divisor;
};
App apps[] = {
    {"Timer",      "DAUER",       "min", KNOB_ICON_TIMER,   0xD5B2FF, 1, 60, 1, 15, 1},
    {"Musik",      "LAUTSTÄRKE",  "%",   KNOB_ICON_MUSIC,   0xAEF582, 0, 100, 5, 75, 1},
    {"Licht",      "HELLIGKEIT",  "%",   KNOB_ICON_SUN,     0xFFCD7C, 0, 100, 5, 60, 1},
    {"Temperatur", "TEMPERATUR",  "°C",  KNOB_ICON_THERMO,  0x86C5FF, 160, 300, 5, 220, 10},
    {"System",     "DISPLAY",     "%",   KNOB_ICON_DISPLAY, 0xB9ACFF, DISPLAY_MIN_PERCENT, DISPLAY_MAX_PERCENT, 5, DISPLAY_DEFAULT_PERCENT, 1},
    {"Mehr",       "",            "",    KNOB_ICON_MORE,    0xC3D0D8, 0, 0, 0, 0, 1}
};
constexpr int APP_COUNT = sizeof(apps) / sizeof(apps[0]);
constexpr int kDisplayApp = 4;
const char *settingKeys[] = {"timer", "volume", "light", "temperature", "backlight"};
enum View { KNOB_VIEW_HOME, KNOB_VIEW_VALUE, KNOB_VIEW_MENU, KNOB_VIEW_STATUS, KNOB_VIEW_STANDBY, VIEW_COUNT };
View state = KNOB_VIEW_HOME, statusReturn = KNOB_VIEW_HOME;
lv_obj_t *root, *views[VIEW_COUNT];
lv_obj_t *cards[APP_COUNT], *dots[APP_COUNT], *homeTitle, *homeValue;
lv_obj_t *valueArc, *valueText, *valueUnit, *valueTitle, *valueIcon;
lv_obj_t *valueHint, *minText, *maxText, *valueContent;
lv_obj_t *menuFocus, *menuRows[4];
lv_obj_t *statusIcon, *statusTitle, *statusSubtitle, *statusRing;
lv_obj_t *standbyTime, *standbyDate, *standbyStatus;
Icon homeIcons[APP_COUNT], editorIcon = KNOB_ICON_MUSIC, statusSymbol = KNOB_ICON_CHECK;
int selected = 1, menuIndex = 0, draftValue = 0;
int32_t carouselPosition = 1000, carouselTarget = 1000;
int32_t displayedArc = 0;
uint32_t lastActivity = 0, statusStarted = 0, statusDuration = 0;
uint32_t hintStarted = 0, lastStandbySecond = UINT32_MAX;
const char *menuNames[] = {"Info", "Status-Demo", "Standby", "Zurück"};
lv_point_t homeTicks[60][2], valueTicks[41][2];

lv_color_t color(uint32_t rgb) { return lv_color_hex(rgb); }
int clampInt(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

lv_obj_t *box(lv_obj_t *parent, int w, int h) {
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(o, w, h);
    return o;
}

lv_obj_t *label(lv_obj_t *parent, const char *text, const lv_font_t *font,
                uint32_t rgb, int y, int width = 180) {
    lv_obj_t *o = lv_label_create(parent);
    lv_label_set_text(o, text);
    lv_label_set_long_mode(o, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(o, width);
    lv_obj_set_style_text_font(o, font, 0);
    lv_obj_set_style_text_color(o, color(rgb), 0);
    lv_obj_set_style_text_align(o, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(o, LV_ALIGN_TOP_MID, 0, y);
    return o;
}

void moveY(void *o, int32_t v) { lv_obj_set_style_translate_y((lv_obj_t *)o, v, 0); }
void moveX(void *o, int32_t v) { lv_obj_set_style_translate_x((lv_obj_t *)o, v, 0); }
void setY(void *o, int32_t v) { lv_obj_set_y((lv_obj_t *)o, v); }
void opacity(void *o, int32_t v) { lv_obj_set_style_opa((lv_obj_t *)o, v, 0); }

void animate(void *target, lv_anim_exec_xcb_t cb, int32_t from, int32_t to,
             uint16_t ms, lv_anim_path_cb_t path = lv_anim_path_ease_out) {
    // LVGL replaces the previous animation for this target/callback pair.
    // Always start at the current visual position, including during reversals.
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, target);
    lv_anim_set_exec_cb(&a, cb);
    lv_anim_set_values(&a, from, to);
    lv_anim_set_time(&a, UI_ANIMATIONS ? ms : 0);
    lv_anim_set_path_cb(&a, path);
    lv_anim_start(&a);
}

// Immediate-mode vector icons: no shared mutable lv_line point buffers,
// no bitmap assets, no allocation when an icon changes size.
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
            for (int i = 0; i < 8; ++i) {
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

void initTicks() {
    for (int i = 0; i < 60; ++i) {
        float a = i * kKnobPi / 30;
        int r = i % 5 == 0 ? 108 : 112;
        homeTicks[i][0] = {(lv_coord_t)lroundf(120 + cosf(a)*r), (lv_coord_t)lroundf(120 + sinf(a)*r)};
        homeTicks[i][1] = {(lv_coord_t)lroundf(120 + cosf(a)*116), (lv_coord_t)lroundf(120 + sinf(a)*116)};
    }
    for (int i = 0; i <= 40; ++i) {
        float a = (135 + i * 270.0f / 40) * kKnobPi / 180;
        int r = i % 5 == 0 ? 108 : 112;
        valueTicks[i][0] = {(lv_coord_t)lroundf(120 + cosf(a)*r), (lv_coord_t)lroundf(120 + sinf(a)*r)};
        valueTicks[i][1] = {(lv_coord_t)lroundf(120 + cosf(a)*116), (lv_coord_t)lroundf(120 + sinf(a)*116)};
    }
}

void ticksDraw(lv_event_t *e) {
    bool editor = lv_event_get_target(e) == views[KNOB_VIEW_VALUE];
    lv_draw_ctx_t *ctx = lv_event_get_draw_ctx(e);
    lv_draw_line_dsc_t d;
    lv_draw_line_dsc_init(&d);
    d.round_start = d.round_end = 1;
    for (int i = 0; i < (editor ? 41 : 60); ++i) {
        d.width = i % 5 == 0 ? 3 : 2;
        d.color = color(i % 5 == 0 ? 0x53616B : TRACK);
        if (editor && displayedArc > 0 && i * 25 <= displayedArc)
            d.color = lv_color_mix(color(apps[selected].color), color(BG), 160);
        lv_point_t *p = editor ? valueTicks[i] : homeTicks[i];
        lv_draw_line(ctx, &d, &p[0], &p[1]);
    }
}

void formatNumber(char *buf, size_t size, int value, const App &a) {
    if (a.divisor == 10) snprintf(buf, size, "%d.%d", value / 10, value % 10);
    else snprintf(buf, size, "%d", value);
}

void updateHomeText() {
    App &a = apps[selected];
    lv_label_set_text(homeTitle, a.name);
    char number[16], text[40];
    formatNumber(number, sizeof(number), a.value, a);
    if (selected == APP_COUNT - 1) snprintf(text, sizeof(text), "OPTIONEN");
    else snprintf(text, sizeof(text), "%s %s", number, a.unit);
    lv_label_set_text(homeValue, text);
    lv_obj_set_style_text_color(homeValue, color(a.color), 0);
    for (int i = 0; i < APP_COUNT; ++i) {
        lv_obj_set_style_bg_color(dots[i], color(i == selected ? a.color : TRACK), 0);
        lv_obj_set_size(dots[i], i == selected ? 14 : 6, 6);
        lv_obj_align(dots[i], LV_ALIGN_TOP_MID, (i * 16) - 40, 211);
    }
}

void carouselFrame(void *, int32_t position) {
    carouselPosition = position;
    for (int i = 0; i < APP_COUNT; ++i) {
        float slot = i - position / 1000.0f;
        while (slot > APP_COUNT / 2.0f) slot -= APP_COUNT;
        while (slot < -APP_COUNT / 2.0f) slot += APP_COUNT;
        float distance = fabsf(slot);
        if (distance >= 1.75f) { lv_obj_add_flag(cards[i], LV_OBJ_FLAG_HIDDEN); continue; }
        lv_obj_clear_flag(cards[i], LV_OBJ_FLAG_HIDDEN);
        float focus = fmaxf(0, 1 - distance);
        int size = (int)(46 + 46 * focus);
        int x = (int)(120 + 91 * sinf(slot * 0.98f));
        int y = (int)(106 + 31 * (1 - cosf(slot * 0.98f)));
        lv_obj_set_size(cards[i], size, size);
        lv_obj_set_pos(cards[i], x - size/2, y - size/2);
        lv_color_t tint = lv_color_mix(color(apps[i].color), color(PANEL), (uint8_t)(24 * focus));
        lv_obj_set_style_bg_color(cards[i], tint, 0);
        lv_obj_set_style_border_color(cards[i], lv_color_mix(color(apps[i].color), color(TRACK), (uint8_t)(220 * focus)), 0);
        lv_obj_t *icon = lv_obj_get_child(cards[i], 0);
        int iconSize = (int)(28 + 18 * focus);
        lv_obj_set_size(icon, iconSize, iconSize);
        lv_obj_center(icon);
        uint8_t strength = (uint8_t)(65 + 190 * focus);
        // Fade by color instead of allocating a translucent layer for every card.
        if (distance > 1) strength = (uint8_t)(65 * (1.75f-distance)/0.75f);
        lv_obj_set_style_text_color(icon, lv_color_mix(color(apps[i].color), color(PANEL), strength), 0);
    }
}

void stepCarousel(int dir) {
    // Normalize together before updating, keeping animation positions bounded.
    int32_t base = (carouselTarget / (APP_COUNT * 1000)) * (APP_COUNT * 1000);
    carouselTarget -= base;
    carouselPosition -= base;
    selected = (selected + dir + APP_COUNT) % APP_COUNT;
    carouselTarget += dir * 1000;
    updateHomeText();
    lv_obj_move_foreground(cards[selected]);
    animate(&carouselPosition, carouselFrame, carouselPosition, carouselTarget, UI_CAROUSEL_MS);
    animate(homeTitle, moveY, dir * 3, 0, UI_CAROUSEL_MS);
}

void arcFrame(void *, int32_t value) {
    int oldTick = displayedArc / 25;
    bool wasZero = displayedArc == 0;
    displayedArc = clampInt(value, 0, 1000);
    lv_arc_set_value(valueArc, displayedArc);
    if (oldTick != displayedArc / 25 || wasZero != (displayedArc == 0))
        lv_obj_invalidate(views[KNOB_VIEW_VALUE]);
}

void updateValue(bool entering = false) {
    App &a = apps[selected];
    char buf[16];
    formatNumber(buf, sizeof(buf), draftValue, a);
    // Numeric target is immediate and exact; only the arc follows with easing.
    lv_label_set_text(valueText, buf);
    int target = (draftValue - a.min) * 1000 / (a.max - a.min);
    animate(&displayedArc, arcFrame, displayedArc, target,
            entering ? UI_ENTER_MS + 100 : UI_VALUE_MS);
}

void showView(View next, int direction = 1) {
    // Persistent views eliminate screen deletion races on fast input.
    // No full-screen alpha/zoom layer (expensive on a 240x240 SPI display).
    for (int i = 0; i < VIEW_COUNT; ++i) lv_obj_add_flag(views[i], LV_OBJ_FLAG_HIDDEN);
    lv_anim_del(&carouselPosition, carouselFrame);
    lv_anim_del(&displayedArc, arcFrame);
    lv_anim_del(statusRing, opacity);
    View previous = state;
    state = next;
    if (next == KNOB_VIEW_STANDBY) {
        lastStandbySecond = UINT32_MAX;
        backlight_set_percent(STANDBY_BRIGHTNESS_PERCENT);
    } else if (previous == KNOB_VIEW_STANDBY) {
        backlight_set_percent(apps[kDisplayApp].value);
    }
    statusDuration = 0;
    lv_obj_clear_flag(views[next], LV_OBJ_FLAG_HIDDEN);
    if (next == KNOB_VIEW_HOME) {
        carouselFrame(NULL, carouselTarget);
        updateHomeText();
        animate(homeTitle, moveY, direction * 8, 0, UI_ENTER_MS);
    } else if (next == KNOB_VIEW_VALUE) {
        lv_anim_del(valueContent, moveX);
        moveX(valueContent, 0);
        animate(valueContent, moveY, 10, 0, UI_ENTER_MS);
    } else if (next == KNOB_VIEW_MENU) {
        animate(menuFocus, setY, lv_obj_get_y(menuFocus), 64 + menuIndex * 31, UI_MENU_MS);
    }
}

void openValue() {
    App &a = apps[selected];
    draftValue = a.value;
    showView(KNOB_VIEW_VALUE);
    editorIcon = a.icon;
    lv_obj_set_style_text_color(valueIcon, color(a.color), 0);
    lv_obj_invalidate(valueIcon);
    lv_label_set_text(valueTitle, a.caption);
    lv_label_set_text(valueUnit, a.unit);
    lv_label_set_text(valueHint, "KLICK: OK");
    lv_obj_set_style_text_color(valueHint, color(MUTED), 0);
    hintStarted = 0;
    char buf[16];
    formatNumber(buf, sizeof(buf), a.min, a); lv_label_set_text(minText, buf);
    formatNumber(buf, sizeof(buf), a.max, a); lv_label_set_text(maxText, buf);
    lv_obj_set_style_arc_color(valueArc, color(a.color), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(valueArc, color(WHITE), LV_PART_KNOB);
    arcFrame(NULL, 0);
    updateValue(true);
}

void turnValue(int direction) {
    App &a = apps[selected];
    int next = clampInt(draftValue + direction * a.step, a.min, a.max);
    if (next == draftValue) {
        lv_label_set_text(valueHint, direction < 0 ? "MINIMUM" : "MAXIMUM");
        lv_obj_set_style_text_color(valueHint, color(a.color), 0);
        hintStarted = millis();
        if (UI_ANIMATIONS) {
            // A 3px elastic nudge signals the stop; the number stays in range.
            lv_anim_t anim;
            lv_anim_init(&anim);
            lv_anim_set_var(&anim, valueContent);
            lv_anim_set_exec_cb(&anim, moveX);
            lv_anim_set_values(&anim, lv_obj_get_style_translate_x(valueContent, 0), direction * 3);
            lv_anim_set_time(&anim, 55);
            lv_anim_set_playback_time(&anim, 100);
            // End at neutral even if another end-stop input interrupts this one.
            lv_anim_del(valueContent, moveX);
            moveX(valueContent, 0);
            lv_anim_set_values(&anim, 0, direction * 3);
            lv_anim_start(&anim);
        }
        return;
    }
    draftValue = next;
    if (selected == kDisplayApp) backlight_set_percent(draftValue);
    lv_label_set_text(valueHint, "KLICK: OK");
    lv_obj_set_style_text_color(valueHint, color(MUTED), 0);
    hintStarted = 0;
    updateValue();
}

void selectMenu(int index) {
    menuIndex = index;
    for (int i = 0; i < 4; ++i)
        lv_obj_set_style_text_color(menuRows[i], color(i == menuIndex ? WHITE : MUTED), 0);
    animate(menuFocus, setY, lv_obj_get_y(menuFocus), 64 + menuIndex * 31, UI_MENU_MS);
}

void showStatus(Icon icon, uint32_t rgb, const char *title, const char *subtitle,
                View destination, uint32_t duration) {
    showView(KNOB_VIEW_STATUS);
    statusSymbol = icon;
    lv_obj_set_style_text_color(statusIcon, color(rgb), 0);
    lv_obj_invalidate(statusIcon);
    lv_obj_set_style_border_color(statusRing, color(rgb), 0);
    lv_label_set_text(statusTitle, title);
    lv_label_set_text(statusSubtitle, subtitle);
    animate(statusRing, opacity, LV_OPA_30, LV_OPA_COVER, UI_ENTER_MS);
    animate(statusTitle, moveY, 6, 0, UI_ENTER_MS);
    statusReturn = destination;
    statusStarted = millis();
    statusDuration = duration;
}

void updateStandby() {
    uint32_t seconds = millis() / 1000;
    if (seconds == lastStandbySecond) return;
    lastStandbySecond = seconds;
    struct tm local;
    bool valid = network_time_local(&local);
    bool online = network_time_connected();
    if (valid) {
        char clock[8], date[24];
        strftime(clock, sizeof(clock), "%H:%M", &local);
        const char *days[] = {"So", "Mo", "Di", "Mi", "Do", "Fr", "Sa"};
        date[0] = days[local.tm_wday][0];
        date[1] = days[local.tm_wday][1];
        date[2] = ',';
        date[3] = ' ';
        strftime(date + 4, sizeof(date) - 4, "%d.%m.%Y", &local);
        lv_label_set_text(standbyTime, clock);
        lv_label_set_text(standbyDate, date);
    } else {
        lv_label_set_text(standbyTime, "--:--");
        lv_label_set_text(standbyDate, online ? "Zeit wird geladen" : "Zeit nicht synchron");
    }
    const char *status = !network_time_configured() ? "WLAN EINTRAGEN"
                       : online ? (valid ? "WLAN VERBUNDEN" : "ZEIT ABGLEICH")
                       : valid ? "WLAN OFFLINE" : "WLAN VERBINDET";
    lv_label_set_text(standbyStatus, status);
    lv_obj_set_style_text_color(standbyStatus, color(online ? 0xAEF582 : MUTED), 0);
}

void buildViews() {
    root = box(NULL, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_set_style_bg_color(root, color(BG), 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    for (int i = 0; i < VIEW_COUNT; ++i) {
        views[i] = box(root, SCREEN_WIDTH, SCREEN_HEIGHT);
        lv_obj_add_flag(views[i], LV_OBJ_FLAG_HIDDEN);
    }
    initTicks();
    lv_obj_add_event_cb(views[KNOB_VIEW_HOME], ticksDraw, LV_EVENT_DRAW_MAIN, NULL);
    lv_obj_add_event_cb(views[KNOB_VIEW_VALUE], ticksDraw, LV_EVENT_DRAW_MAIN, NULL);
    label(views[KNOB_VIEW_HOME], "SMART KNOB", &knob_font_14, MUTED, 34, 140);
    for (int i = 0; i < APP_COUNT; ++i) {
        cards[i] = box(views[KNOB_VIEW_HOME], 84, 84);
        lv_obj_set_style_radius(cards[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(cards[i], LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(cards[i], 2, 0);
        homeIcons[i] = apps[i].icon;
        makeIcon(cards[i], &homeIcons[i], apps[i].color, 40);
        dots[i] = box(views[KNOB_VIEW_HOME], 4, 4);
        lv_obj_set_style_radius(dots[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(dots[i], LV_OPA_COVER, 0);
    }
    homeTitle = label(views[KNOB_VIEW_HOME], "", &knob_font_24, WHITE, 156, 186);
    homeValue = label(views[KNOB_VIEW_HOME], "", &knob_font_16, MUTED, 184, 154);

    valueArc = lv_arc_create(views[KNOB_VIEW_VALUE]);
    lv_obj_remove_style_all(valueArc);
    lv_obj_set_size(valueArc, 204, 204);
    lv_obj_center(valueArc);
    lv_arc_set_rotation(valueArc, 135);
    lv_arc_set_bg_angles(valueArc, 0, 270);
    lv_arc_set_range(valueArc, 0, 1000);
    lv_arc_set_change_rate(valueArc, 10000);
    lv_obj_set_style_arc_width(valueArc, 10, LV_PART_MAIN);
    lv_obj_set_style_arc_color(valueArc, color(TRACK), LV_PART_MAIN);
    lv_obj_set_style_arc_opa(valueArc, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_arc_width(valueArc, 10, LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(valueArc, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(valueArc, true, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(valueArc, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_radius(valueArc, LV_RADIUS_CIRCLE, LV_PART_KNOB);
    lv_obj_set_style_pad_all(valueArc, 1, LV_PART_KNOB);
    lv_obj_clear_flag(valueArc, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    valueContent = box(views[KNOB_VIEW_VALUE], 160, 158);
    lv_obj_set_pos(valueContent, 40, 40);
    valueTitle = label(valueContent, "", &knob_font_14, MUTED, 9, 156);
    valueIcon = makeIcon(valueContent, &editorIcon, apps[1].color, 34);
    lv_obj_align(valueIcon, LV_ALIGN_TOP_MID, 0, 31);
    valueText = label(valueContent, "", &knob_font_52, WHITE, 68, 156);
    valueUnit = label(valueContent, "", &knob_font_18, MUTED, 113, 100);
    label(valueContent, "DREHEN", &knob_font_14, MUTED, 138, 100);
    minText = label(views[KNOB_VIEW_VALUE], "", &knob_font_14, MUTED, 192, 50);
    lv_obj_align(minText, LV_ALIGN_TOP_MID, -64, 192);
    maxText = label(views[KNOB_VIEW_VALUE], "", &knob_font_14, MUTED, 192, 50);
    lv_obj_align(maxText, LV_ALIGN_TOP_MID, 64, 192);
    valueHint = label(views[KNOB_VIEW_VALUE], "KLICK: OK", &knob_font_14, MUTED, 210, 116);

    label(views[KNOB_VIEW_MENU], "MEHR", &knob_font_14, MUTED, 36, 120);
    menuFocus = box(views[KNOB_VIEW_MENU], 156, 29);
    lv_obj_set_pos(menuFocus, 42, 64);
    lv_obj_set_style_radius(menuFocus, 10, 0);
    lv_obj_set_style_bg_color(menuFocus, color(0x253A36), 0);
    lv_obj_set_style_bg_opa(menuFocus, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(menuFocus, 2, 0);
    lv_obj_set_style_border_color(menuFocus, color(0x58745D), 0);
    for (int i = 0; i < 4; ++i)
        menuRows[i] = label(views[KNOB_VIEW_MENU], menuNames[i], &knob_font_16, MUTED, 69 + i * 31, 148);
    label(views[KNOB_VIEW_MENU], "HALTEN: ZURÜCK", &knob_font_14, MUTED, 202, 164);

    statusRing = box(views[KNOB_VIEW_STATUS], 90, 90);
    lv_obj_align(statusRing, LV_ALIGN_TOP_MID, 0, 40);
    lv_obj_set_style_radius(statusRing, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(statusRing, 3, 0);
    statusIcon = makeIcon(statusRing, &statusSymbol, apps[1].color, 48);
    statusTitle = label(views[KNOB_VIEW_STATUS], "", &knob_font_24, WHITE, 141, 198);
    statusSubtitle = label(views[KNOB_VIEW_STATUS], "", &knob_font_14, MUTED, 177, 194);

    standbyStatus = label(views[KNOB_VIEW_STANDBY], "WLAN EINTRAGEN", &knob_font_14, MUTED, 58, 176);
    standbyTime = label(views[KNOB_VIEW_STANDBY], "--:--", &knob_font_52, 0xADBCB7, 91, 200);
    standbyDate = label(views[KNOB_VIEW_STANDBY], "Zeit nicht synchron", &knob_font_14, MUTED, 146, 180);
    label(views[KNOB_VIEW_STANDBY], "DREHEN ZUM WECKEN", &knob_font_14, MUTED, 185, 184);
    lv_scr_load(root);
}
} // namespace

void ui_init() {
    for (int i = 0; i < APP_COUNT - 1; ++i) {
        App &a = apps[i];
        a.value = settings_load(settingKeys[i], a.value, a.min, a.max, a.step);
    }
    buildViews();
    showView(KNOB_VIEW_HOME);
    selectMenu(0);
    lastActivity = millis();
    // Render once before enabling the stored brightness; no full-bright boot flash.
    lv_refr_now(NULL);
    backlight_set_percent(apps[kDisplayApp].value, false);
}

void ui_notify_activity() { lastActivity = millis(); }

void ui_check_idle() {
    uint32_t now = millis();
    // Subtractions are safe across millis() rollover.
    if (state == KNOB_VIEW_HOME && (uint32_t)(now - lastActivity) >= IDLE_TIMEOUT_MS) {
        showView(KNOB_VIEW_STANDBY);
        updateStandby();
    }
    if (state == KNOB_VIEW_STATUS && statusDuration && (uint32_t)(now - statusStarted) >= statusDuration) {
        showView(statusReturn);
        ui_notify_activity();
    }
    if (state == KNOB_VIEW_VALUE && hintStarted && (uint32_t)(now - hintStarted) >= 700) {
        hintStarted = 0;
        lv_label_set_text(valueHint, "KLICK: OK");
        lv_obj_set_style_text_color(valueHint, color(MUTED), 0);
    }
    if (state == KNOB_VIEW_STANDBY) updateStandby();
}

void ui_handle_input(InputEvent evt) {
    if (evt == EVT_NONE) return;
    ui_notify_activity();
    int dir = evt == EVT_LEFT ? -1 : 1;
    bool turn = evt == EVT_LEFT || evt == EVT_RIGHT;
    switch (state) {
        case KNOB_VIEW_HOME:
            if (turn) stepCarousel(dir);
            else if (evt == EVT_PRESS_SHORT) {
                if (selected == APP_COUNT - 1) { selectMenu(0); showView(KNOB_VIEW_MENU); }
                else openValue();
            } else if (evt == EVT_PRESS_LONG) { showView(KNOB_VIEW_STANDBY); updateStandby(); }
            break;
        case KNOB_VIEW_VALUE:
            if (turn) turnValue(dir);
            else if (evt == EVT_PRESS_SHORT) {
                if (settings_save(settingKeys[selected], draftValue)) {
                    apps[selected].value = draftValue;
                    showStatus(KNOB_ICON_CHECK, apps[selected].color, "Gespeichert", "Bleibt nach Neustart", KNOB_VIEW_HOME, 800);
                } else {
                    if (selected == kDisplayApp) backlight_set_percent(apps[kDisplayApp].value);
                    showStatus(KNOB_ICON_CROSS, 0xFF8B86, "Speicherfehler", "Wert nicht gespeichert", KNOB_VIEW_HOME, 1800);
                }
            } else if (evt == EVT_PRESS_LONG) {
                // Editing is transactional: discard the draft, retain committed value.
                if (selected == kDisplayApp) backlight_set_percent(apps[kDisplayApp].value);
                showView(KNOB_VIEW_HOME, -1);
            }
            break;
        case KNOB_VIEW_MENU:
            if (turn) selectMenu((menuIndex + dir + 4) % 4);
            else if (evt == EVT_PRESS_LONG) showView(KNOB_VIEW_HOME, -1);
            else if (evt == EVT_PRESS_SHORT) {
                if (menuIndex == 0) showStatus(KNOB_ICON_INFO, 0x86C5FF, "Smart Knob", "ESP32-S3  /  LVGL 8.3", KNOB_VIEW_MENU, 2200);
                else if (menuIndex == 1) showStatus(KNOB_ICON_CROSS, 0xFF8B86, "Status-Demo", "Keine echte Fehlermeldung", KNOB_VIEW_MENU, 1800);
                else if (menuIndex == 2) { showView(KNOB_VIEW_STANDBY); updateStandby(); }
                else showView(KNOB_VIEW_HOME, -1);
            }
            break;
        case KNOB_VIEW_STATUS:
            // Dismiss early; consume this input to avoid accidental value changes.
            showView(statusReturn);
            break;
        case KNOB_VIEW_STANDBY:
            // Wake gesture is consumed, not applied as a turn/click.
            showView(KNOB_VIEW_HOME);
            break;
        default: break;
    }
}

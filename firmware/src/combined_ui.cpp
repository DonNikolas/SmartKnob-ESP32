// SmartKnob combined display / LED view. Generated with ChatGPT assistance.
// New integration; hardware validation pending.
#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <Adafruit_NeoPixel.h>
#include <lvgl.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <math.h>
#include "combined_ui.h"

#if !CONFIG_IDF_TARGET_ESP32S3 || ARDUINO_RUNNING_CORE != 1
#error "This integration requires ESP32-S3 with the Arduino loop on core 1."
#endif
#if LVGL_VERSION_MAJOR != 8 || LVGL_VERSION_MINOR != 3
#error "Use LVGL 8.3.x with the existing include/lv_conf.h."
#endif

namespace CombinedUi {
namespace {
constexpr uint32_t UI_BACKGROUND = 0x080C10, UI_TEXT = 0xEDF4EF;
constexpr uint32_t UI_ACCENT = 0x86C5FF, UI_CONFIRM = 0x55D98B, UI_DIM = 0x26343F;
constexpr uint8_t BL = 9, BL_CHANNEL = 0;
// Increased from the initial 1 MHz test; verify image integrity on the PCB.
constexpr uint32_t DISPLAY_SPI_HZ = 8000000UL;

// Arduino-ESP32 2.0.17 on S3: HSPI selects physical SPI3.
// Encoder remains on ESP-IDF SPI2_HOST. Do not use GFX's FSPI default here.
Arduino_ESP32SPI bus(18, 42, 3, 8, GFX_NOT_DEFINED, HSPI);
Arduino_GC9A01 display(&bus, 10, 0, true);
Adafruit_NeoPixel ring(LED_COUNT, 11, NEO_GRB + NEO_KHZ800);
QueueHandle_t states = nullptr, initResult = nullptr;
lv_color_t pixels[240 * 10];
lv_disp_draw_buf_t drawBuffer;
lv_disp_drv_t displayDriver;
lv_obj_t *number, *status, *confirmation, *dots[POSITIONS];

void flush(lv_disp_drv_t* driver, const lv_area_t* area, lv_color_t* colors) {
    display.draw16bitRGBBitmap(area->x1, area->y1,
        reinterpret_cast<uint16_t*>(colors),
        area->x2 - area->x1 + 1, area->y2 - area->y1 + 1);
    lv_disp_flush_ready(driver);
}
lv_obj_t* label(const char* text, const lv_font_t* font, int y) {
    lv_obj_t* item = lv_label_create(lv_scr_act());
    lv_label_set_text(item, text);
    lv_obj_set_style_text_font(item, font, 0);
    lv_obj_set_style_text_color(item, lv_color_hex(UI_TEXT), 0);
    lv_obj_align(item, LV_ALIGN_CENTER, 0, y);
    return item;
}
void makeScreen() {
    lv_obj_t* screen = lv_scr_act();
    lv_obj_set_style_bg_color(screen, lv_color_hex(UI_BACKGROUND), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(screen, 0, 0);
    label("POSITION", &lv_font_montserrat_16, -47);
    number = label("01", &lv_font_montserrat_48, -7);
    status = label("Motor off", &lv_font_montserrat_16, 35);
    confirmation = label("Press to confirm", &lv_font_montserrat_16, 59);
    for (int i = 0; i < POSITIONS; ++i) {
        // Front view: position 0 at 6 o'clock; 1 towards 5 o'clock (CCW).
        const float angle = (90.0f - i * 30.0f) * 0.01745329252f;
        dots[i] = lv_obj_create(screen);
        lv_obj_remove_style_all(dots[i]);
        lv_obj_set_size(dots[i], 9, 9);
        lv_obj_set_pos(dots[i], 120 + lroundf(99 * cosf(angle)) - 4,
                                   120 + lroundf(99 * sinf(angle)) - 4);
        lv_obj_set_style_radius(dots[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(dots[i], LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(dots[i], lv_color_hex(UI_DIM), 0);
    }
}
const char* stateText(const State& s) {
    if (s.alignment) return "Check direction";
    if (s.running) return "Turn / press";
    if (s.zeroing) return "Release knob";
    if (!s.pressureReady) return "Zero with t";
    if (!s.centred) return "Centre with c";
    return "Motor off";
}
void updateRing(const State& s, bool flash) {
    ring.clear();
    if (!s.alignment && s.confirmed >= 0)
        ring.setPixelColor(ledIndex(s.confirmed, s.bottomLed, s.ledDirection),
                           ring.Color(0, 110, 35));
    const uint32_t colour = s.alignment ? ring.Color(180, 180, 180)
                          : flash ? ring.Color(30, 255, 70)
                                  : ring.Color(80, 160, 255);
    ring.setPixelColor(ledIndex(s.position, s.bottomLed, s.ledDirection), colour);
    ring.show();
}
uint32_t dotColour(const State& s, int index) {
    return index == s.position ? UI_ACCENT
         : index == s.confirmed ? UI_CONFIRM : UI_DIM;
}
void task(void*) {
    // Initialize RMT on this core, away from the motor control loop.
    ring.begin(); ring.setBrightness(20); ring.clear(); ring.show();
    bool ok = display.begin(DISPLAY_SPI_HZ);
    if (!ok) {
        xQueueSend(initResult, &ok, 0);
        vTaskDelete(nullptr);
        return;
    }
    lv_init();
    lv_disp_draw_buf_init(&drawBuffer, pixels, nullptr, 240 * 10);
    lv_disp_drv_init(&displayDriver);
    displayDriver.hor_res = displayDriver.ver_res = 240;
    displayDriver.draw_buf = &drawBuffer;
    displayDriver.flush_cb = flush;
    if (!lv_disp_drv_register(&displayDriver)) {
        ok = false; xQueueSend(initResult, &ok, 0); vTaskDelete(nullptr); return;
    }
    makeScreen();
    // Full first frame is drawn before begin returns and motor setup finishes.
    lv_timer_handler();
    // Motor uses MCPWM; this LEDC channel only drives the backlight.
    ledcSetup(BL_CHANNEL, 5000, 10);
    ledcAttachPin(BL, BL_CHANNEL);
    ledcWrite(BL_CHANNEL, 818);
    xQueueSend(initResult, &ok, 0);

    State state, shown;
    bool first = true, lastFlash = false;
    uint32_t previousTick = millis(), lastConfirmation = 0, flashAt = 0;
    bool flashActive = false;
    for (;;) {
        State next;
        if (xQueueReceive(states, &next, 0) == pdTRUE) state = next;
        const uint32_t now = millis();
        if (state.confirmations != lastConfirmation) {
            lastConfirmation = state.confirmations;
            flashAt = now; flashActive = true;
        }
        const bool flash = flashActive && now - flashAt < 400;
        if (!flash) flashActive = false;
        if (first || state.position != shown.position || state.confirmed != shown.confirmed ||
            state.bottomLed != shown.bottomLed || state.ledDirection != shown.ledDirection ||
            state.alignment != shown.alignment || flash != lastFlash) {
            updateRing(state, flash);
        }
        // Only invalidate objects whose visible content actually changed.
        // Recolouring every dot on each step needlessly redraws the whole ring.
        if (first || state.position != shown.position) {
            char text[32];
            snprintf(text, sizeof(text), "%02u", unsigned(state.position + 1));
            lv_label_set_text(number, text);
        }
        if (first || flash != lastFlash) {
            lv_obj_set_style_text_color(number, lv_color_hex(flash ? UI_CONFIRM : UI_TEXT), 0);
        }
        for (int i = 0; i < POSITIONS; ++i) {
            const uint32_t colour = dotColour(state, i);
            if (first || colour != dotColour(shown, i))
                lv_obj_set_style_bg_color(dots[i], lv_color_hex(colour), 0);
        }
        if (first || state.confirmed != shown.confirmed) {
            char text[32];
            if (state.confirmed >= 0) {
                snprintf(text, sizeof(text), "Confirmed: %02u", unsigned(state.confirmed + 1));
                lv_label_set_text(confirmation, text);
            } else lv_label_set_text(confirmation, "Press to confirm");
        }
        if (first || stateText(state) != stateText(shown))
            lv_label_set_text(status, stateText(state));
        shown = state; first = false; lastFlash = flash;
        lv_tick_inc(now - previousTick); previousTick = now;
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
} // namespace

bool begin() {
    states = xQueueCreate(1, sizeof(State));
    initResult = xQueueCreate(1, sizeof(bool));
    if (!states || !initResult) return false;
    if (xTaskCreatePinnedToCore(task, "knob_ui", 8192, nullptr, 1, nullptr, 0) != pdPASS)
        return false;
    bool ok = false;
    if (xQueueReceive(initResult, &ok, pdMS_TO_TICKS(15000)) != pdTRUE) return false;
    return ok;
}
void publish(const State& state) {
    if (states) xQueueOverwrite(states, &state);
}
} // namespace CombinedUi

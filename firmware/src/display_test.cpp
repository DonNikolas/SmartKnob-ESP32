// Generated with ChatGPT assistance based on my project requirements.
// Tested on the current SmartKnob PCB at 1 MHz SPI after resoldering.

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <lvgl.h>

#if LVGL_VERSION_MAJOR != 8 || LVGL_VERSION_MINOR != 3
#error "This test requires LVGL 8.3.x."
#endif

// Current SmartKnob PCB pin assignments.
constexpr uint8_t PIN_SCK = 3;
constexpr uint8_t PIN_MOSI = 8;
constexpr uint8_t PIN_DC = 18;
constexpr uint8_t PIN_CS = 42;
constexpr uint8_t PIN_RST = 10;
constexpr uint8_t PIN_BL = 9;

constexpr uint8_t MOTOR_PINS[] = {4, 7, 5, 16, 6, 15};

constexpr uint16_t SCREEN_SIZE = 240;
constexpr uint8_t BACKLIGHT_CHANNEL = 0;
constexpr uint32_t BACKGROUND = 0x080C10;
constexpr uint32_t TEXT_COLOR = 0xEDF4EF;
constexpr uint32_t ACCENT = 0x86C5FF;

// Start with a moderate SPI speed for the hardware test.
Arduino_DataBus* bus = new Arduino_ESP32SPI(
    PIN_DC, PIN_CS, PIN_SCK, PIN_MOSI, GFX_NOT_DEFINED
);

Arduino_GFX* display = new Arduino_GC9A01(
    bus, PIN_RST, 0, true
);

static lv_color_t pixels[SCREEN_SIZE * 40];
static lv_disp_draw_buf_t drawBuffer;
static lv_disp_drv_t displayDriver;

static lv_obj_t* valueLabel = nullptr;
static lv_obj_t* valueArc = nullptr;
static uint32_t previousTick = 0;

void setBrightness(uint8_t percent) {
    percent = constrain(percent, 10, 100);
    ledcWrite(BACKLIGHT_CHANNEL, percent * 1023UL / 100);
}

void flushDisplay(lv_disp_drv_t* driver,
                  const lv_area_t* area,
                  lv_color_t* colors) {
    const uint16_t width = area->x2 - area->x1 + 1;
    const uint16_t height = area->y2 - area->y1 + 1;

    display->draw16bitRGBBitmap(
        area->x1, area->y1,
        reinterpret_cast<uint16_t*>(colors),
        width, height
    );

    lv_disp_flush_ready(driver);
}

// Keep LVGL running while a test screen is visible.
void showFor(uint32_t duration) {
    const uint32_t started = millis();

    do {
        const uint32_t now = millis();
        lv_tick_inc(now - previousTick);
        previousTick = now;
        lv_timer_handler();
        delay(5);
    } while (millis() - started < duration);
}

void clearScreen(uint32_t background) {
    lv_obj_t* screen = lv_scr_act();
    lv_obj_clean(screen);
    valueLabel = nullptr;
    valueArc = nullptr;

    lv_obj_set_style_bg_color(screen, lv_color_hex(background), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(screen, 0, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
}

lv_obj_t* addLabel(const char* text, const lv_font_t* font,
                   int x, int y) {
    lv_obj_t* label = lv_label_create(lv_scr_act());
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(TEXT_COLOR), 0);
    lv_obj_align(label, LV_ALIGN_CENTER, x, y);
    return label;
}

void showColor(const char* name, uint32_t color) {
    Serial.printf("Colour screen: %s\n", name);
    clearScreen(color);
    showFor(2000);
}

void showGeometry() {
    Serial.println("Check circle, orientation and font sizes.");
    clearScreen(BACKGROUND);

    lv_obj_t* circle = lv_obj_create(lv_scr_act());
    lv_obj_remove_style_all(circle);
    lv_obj_set_size(circle, 220, 220);
    lv_obj_center(circle);
    lv_obj_set_style_radius(circle, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(circle, 3, 0);
    lv_obj_set_style_border_color(circle, lv_color_hex(ACCENT), 0);
    lv_obj_clear_flag(circle, LV_OBJ_FLAG_SCROLLABLE);

    addLabel("TOP", &lv_font_montserrat_16, 0, -90);
    addLabel("BOTTOM", &lv_font_montserrat_16, 0, 90);
    addLabel("L", &lv_font_montserrat_16, -92, 0);
    addLabel("R", &lv_font_montserrat_16, 92, 0);

    addLabel("16 px", &lv_font_montserrat_16, 0, -52);
    addLabel("24 px", &lv_font_montserrat_24, 0, -23);
    addLabel("123", &lv_font_montserrat_48, 0, 24);

    showFor(6000);
}

void buildArcScreen(const char* title) {
    clearScreen(BACKGROUND);

    valueArc = lv_arc_create(lv_scr_act());
    lv_obj_set_size(valueArc, 210, 210);
    lv_obj_center(valueArc);
    lv_arc_set_rotation(valueArc, 135);
    lv_arc_set_bg_angles(valueArc, 0, 270);
    lv_arc_set_range(valueArc, 0, 100);
    lv_arc_set_value(valueArc, 0);

    lv_obj_remove_style(valueArc, nullptr, LV_PART_KNOB);
    lv_obj_clear_flag(valueArc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(valueArc, 10, LV_PART_MAIN);
    lv_obj_set_style_arc_width(valueArc, 10, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(
        valueArc, lv_color_hex(0x27323B), LV_PART_MAIN
    );
    lv_obj_set_style_arc_color(
        valueArc, lv_color_hex(ACCENT), LV_PART_INDICATOR
    );
    lv_obj_set_style_arc_rounded(valueArc, true, LV_PART_INDICATOR);

    addLabel(title, &lv_font_montserrat_16, 0, -40);
    valueLabel = addLabel("0", &lv_font_montserrat_48, 0, 8);
    addLabel("%", &lv_font_montserrat_24, 0, 52);
}

void setTestValue(int value) {
    lv_arc_set_value(valueArc, value);
    lv_label_set_text_fmt(valueLabel, "%d", value);
    lv_obj_align(valueLabel, LV_ALIGN_CENTER, 0, 8);
}

void setup() {
    // Keep the motor driver inputs low.
    for (uint8_t pin : MOTOR_PINS) {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, LOW);
    }

    pinMode(PIN_BL, OUTPUT);
    digitalWrite(PIN_BL, LOW);

    Serial.begin(115200);
    delay(2000);
    Serial.println("\n=== SmartKnob display test ===");

    if (!display->begin(1000000UL)) {
        Serial.println("FAIL: Display driver initialization failed.");
        while (true) {
            delay(1000);
        }
    }

    display->fillScreen(0x0000);

    // LEDC API for the Arduino 2.x framework used by this project.
    ledcSetup(BACKLIGHT_CHANNEL, 5000, 10);
    ledcAttachPin(PIN_BL, BACKLIGHT_CHANNEL);
    ledcWrite(BACKLIGHT_CHANNEL, 0);

    lv_init();
    lv_disp_draw_buf_init(
        &drawBuffer, pixels, nullptr, SCREEN_SIZE * 40
    );

    lv_disp_drv_init(&displayDriver);
    displayDriver.hor_res = SCREEN_SIZE;
    displayDriver.ver_res = SCREEN_SIZE;
    displayDriver.flush_cb = flushDisplay;
    displayDriver.draw_buf = &drawBuffer;
    lv_disp_drv_register(&displayDriver);

    previousTick = millis();
    clearScreen(BACKGROUND);
    showFor(50);
    setBrightness(80);

    Serial.println("Driver ready. Check the actual display visually.");
}

void loop() {
    setBrightness(80);

    showColor("RED", 0xFF0000);
    showColor("GREEN", 0x00FF00);
    showColor("BLUE", 0x0000FF);
    showColor("WHITE", 0xFFFFFF);
    showColor("BLACK", 0x000000);

    showGeometry();

    Serial.println("Arc test: 0 to 100 and back.");
    buildArcScreen("ARC TEST");

    for (int value = 0; value <= 100; value++) {
        setTestValue(value);
        showFor(30);
    }

    for (int value = 100; value >= 0; value--) {
        setTestValue(value);
        showFor(30);
    }

    Serial.println("Backlight test: 10% to 100%.");
    buildArcScreen("BRIGHTNESS");

    for (int percent = 10; percent <= 100; percent += 10) {
        Serial.printf("Brightness: %d%%\n", percent);
        setTestValue(percent);
        setBrightness(percent);
        showFor(1000);
    }

    Serial.println("Sequence finished. Repeating.");
}
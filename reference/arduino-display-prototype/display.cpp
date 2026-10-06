#include "display.h"
#include "config.h"
#include "backlight.h"
#include <Arduino.h>
#include <Arduino_GFX_Library.h>

// ---------------------------------------------------------------------
// GC9A01 240x240 rund, ueber Hardware-SPI angesprochen.
// Arduino_GFX_Library uebernimmt den kompletten Treiber, Pins kommen
// 1:1 aus config.h.
// ---------------------------------------------------------------------
static Arduino_DataBus *bus = new Arduino_ESP32SPI(
    PIN_DC, PIN_CS, PIN_SCK, PIN_MOSI, GFX_NOT_DEFINED /* MISO */);

static Arduino_GFX *gfx = new Arduino_GC9A01(
    bus, PIN_RST, 0 /* rotation */, true /* IPS */);

static lv_disp_draw_buf_t draw_buf;
// Zeichenpuffer: 80 Zeilen = 38.400 Bytes; kein PSRAM erforderlich
static lv_color_t buf1[SCREEN_WIDTH * (SCREEN_HEIGHT / 3)];
static lv_disp_drv_t disp_drv;
static uint32_t lastTickMs = 0;

static void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)&color_p->full, w, h);
    lv_disp_flush_ready(disp);
}

bool display_init() {
    pinMode(PIN_BL, OUTPUT);
    digitalWrite(PIN_BL, BACKLIGHT_ACTIVE_HIGH ? LOW : HIGH);

    if (!gfx->begin(DISPLAY_SPI_HZ)) {
        Serial.println("GC9A01-Initialisierung fehlgeschlagen. Pins/Treiber pruefen.");
        return false;
    }
    gfx->fillScreen(0x0000); // Schwarz (RGB565)
    if (!backlight_init()) return false;

    lv_init();
    lv_disp_draw_buf_init(&draw_buf, buf1, NULL, SCREEN_WIDTH * (SCREEN_HEIGHT / 3));

    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = SCREEN_WIDTH;
    disp_drv.ver_res = SCREEN_HEIGHT;
    disp_drv.antialiasing = 1; // Smooth edges at native RGB565 resolution.
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);
    lastTickMs = millis();
    return true;
}

void display_lvgl_tick_and_handler() {
    uint32_t now = millis();
#if !LV_TICK_CUSTOM
    lv_tick_inc((uint32_t)(now - lastTickMs));
#endif
    lastTickMs = now;
    lv_timer_handler();
}

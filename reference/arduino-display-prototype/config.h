#ifndef CONFIG_H
#define CONFIG_H

// ---------------------------------------------------------------------
// Display: 1.28" GC9A01, 240x240, rund
// Verkabelung laut Vorgabe:
//   SCL (SCK)  -> GPIO12
//   SDA (MOSI) -> GPIO11
//   CMD (DC)   -> GPIO10
//   CS         -> GPIO9
//   RES (RST)  -> GPIO8
//   BL         -> GPIO7
// ---------------------------------------------------------------------
#define PIN_SCK   12
#define PIN_MOSI  11
#define PIN_DC    10
#define PIN_CS    9
#define PIN_RST   8
#define PIN_BL    7

#define SCREEN_WIDTH  240
#define SCREEN_HEIGHT 240

// ---------------------------------------------------------------------
// Simulierter Encoder (solange der Motor/echte Encoder nicht dran ist)
//   Links drehen  -> GPIO45 auf GND
//   Rechts drehen -> GPIO0  auf GND
//   Druecken      -> GPIO35 auf GND
//
// ACHTUNG (WROOM-1U mit PSRAM):
//   GPIO0  ist ein Strapping-Pin (Boot-Taster). Beim Einschalten darf er
//   nicht auf GND liegen, sonst startet der ESP32 im Download-Modus.
//   GPIO45 ist ein Strapping-Pin (VDD_SPI). Externe Pegel beim Reset
//   muessen zur Flash-Spannung und eFuse-Konfiguration passen.
//   GPIO35 ist bei Modulen mit Octal-PSRAM (z.B. N8R8/N16R8) intern fuer
//   das PSRAM belegt und steht dann NICHT als freier GPIO zur Verfuegung.
//   Falls dein Modul Octal-PSRAM hat, PIN_BTN_PRESS auf einen anderen
//   freien Pin (z.B. GPIO4) verlegen.
// ---------------------------------------------------------------------
#define PIN_ROT_LEFT    45
#define PIN_ROT_RIGHT   0
#define PIN_BTN_PRESS   35

#define DEBOUNCE_MS       20
#define LONGPRESS_MS      600
#define IDLE_TIMEOUT_MS   20000UL

// Hold either rotation test button to repeat steps. No encoder acceleration:
// each event still corresponds to exactly one configured value step.
#define INPUT_REPEAT_ENABLED  1
#define INPUT_REPEAT_DELAY_MS 400UL
#define INPUT_REPEAT_MS       90UL

// Motion tuning. Set UI_ANIMATIONS to 0 for immediate visual changes.
#define UI_ANIMATIONS    1
#define UI_CAROUSEL_MS   180
#define UI_VALUE_MS      110
#define UI_ENTER_MS      180
#define UI_MENU_MS       120
#define DISPLAY_SPI_HZ   40000000UL

// PWM backlight on PIN_BL. This pin must drive the module's BL/PWM control
// input or a suitable transistor, not a bare high-current LED directly.
#define DISPLAY_MIN_PERCENT 10
#define DISPLAY_MAX_PERCENT 100
#define DISPLAY_DEFAULT_PERCENT 80
#define STANDBY_BRIGHTNESS_PERCENT 10
#define BACKLIGHT_ACTIVE_HIGH 1
#define BACKLIGHT_PWM_HZ 5000
#define BACKLIGHT_PWM_BITS 10
#define BACKLIGHT_FADE_MS 300

#if DISPLAY_MIN_PERCENT < 10 || DISPLAY_MAX_PERCENT > 100 || DISPLAY_MIN_PERCENT > DISPLAY_MAX_PERCENT
#error "Display-Helligkeitsbereich muss innerhalb 10..100 Prozent liegen."
#endif
#if BACKLIGHT_FADE_MS <= 0
#error "BACKLIGHT_FADE_MS muss positiv sein."
#endif
#if STANDBY_BRIGHTNESS_PERCENT < DISPLAY_MIN_PERCENT || STANDBY_BRIGHTNESS_PERCENT > DISPLAY_MAX_PERCENT
#error "Standby-Helligkeit muss zwischen 10 und 100 Prozent liegen."
#endif

#if SCREEN_WIDTH != 240 || SCREEN_HEIGHT != 240
#error "Das runde Layout ist fuer 240x240 Pixel ausgelegt."
#endif

#endif

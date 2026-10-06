// Generated with ChatGPT assistance based on my project requirements.
// Built, uploaded and tested on my SmartKnob PCB by me.

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

constexpr uint8_t LED_PIN = 11;
constexpr uint16_t LED_COUNT = 36;
constexpr uint8_t LED_BRIGHTNESS = 20;

// Keep all six motor control inputs low during this test.
constexpr uint8_t MOTOR_PINS[] = {4, 7, 5, 16, 6, 15};
constexpr uint8_t BACKLIGHT_PIN = 9;

Adafruit_NeoPixel ring(
    LED_COUNT,
    LED_PIN,
    NEO_GRB + NEO_KHZ800
);

void runChase(uint32_t color) {
    for (uint16_t i = 0; i < LED_COUNT; i++) {
        ring.clear();
        ring.setPixelColor(i, color);
        ring.show();

        Serial.printf("LED %u/%u\n", i + 1, LED_COUNT);
        delay(120);
    }

    ring.clear();
    ring.show();
    delay(400);
}

void showRing(uint32_t color) {
    ring.fill(color);
    ring.show();
    delay(1500);

    ring.clear();
    ring.show();
    delay(400);
}

void setup() {
    for (uint8_t pin : MOTOR_PINS) {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, LOW);
    }

    pinMode(BACKLIGHT_PIN, OUTPUT);
    digitalWrite(BACKLIGHT_PIN, LOW);

    Serial.begin(115200);

    ring.begin();
    ring.setBrightness(LED_BRIGHTNESS);
    ring.clear();
    ring.show();

    delay(2000);
    Serial.println("SmartKnob LED test: 36 LEDs on GPIO 11.");
}

void loop() {
    Serial.println("Chase: red");
    runChase(ring.Color(255, 0, 0));

    Serial.println("Chase: green");
    runChase(ring.Color(0, 255, 0));

    Serial.println("Chase: blue");
    runChase(ring.Color(0, 0, 255));

    Serial.println("Full ring: red");
    showRing(ring.Color(255, 0, 0));

    Serial.println("Full ring: green");
    showRing(ring.Color(0, 255, 0));

    Serial.println("Full ring: blue");
    showRing(ring.Color(0, 0, 255));
}
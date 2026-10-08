// Generated with ChatGPT assistance based on my project requirements.
// Experimental pressure-button test. No force calibration.

#include <Arduino.h>
#include <HX711.h>
#include <Adafruit_NeoPixel.h>
#include <stdint.h>
#include <stdlib.h>

constexpr uint8_t HX_DOUT = 12;
constexpr uint8_t HX_SCK = 13;
constexpr uint8_t MOTOR_PINS[] = {4, 7, 5, 16, 6, 15};
constexpr uint8_t BACKLIGHT_PIN = 9;

constexpr uint8_t LED_PIN = 11;
constexpr uint16_t LED_COUNT = 36;

constexpr uint16_t ZERO_SAMPLES = 32;
constexpr unsigned FILTER_SIZE = 8;
constexpr uint32_t PRESS_CONFIRM_MS = 50;
constexpr uint32_t RELEASE_CONFIRM_MS = 150;

HX711 adc;
Adafruit_NeoPixel ring(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

long threshold = 20000;
constexpr long THRESHOLD_STEP = 1000;

bool lightOn = false;
bool zeroValid = false;
bool collectingZero = false;
bool armed = false;
bool pressTiming = false;
bool releaseTiming = false;

long zeroOffset = 0;
int64_t zeroSum = 0;
uint16_t zeroCount = 0;

long filterValues[FILTER_SIZE] = {};
int64_t filterSum = 0;
unsigned filterIndex = 0;
unsigned filterCount = 0;

uint32_t pressStarted = 0;
uint32_t releaseStarted = 0;
uint32_t zeroStarted = 0;
uint32_t lastData = 0;
uint32_t lastWarning = 0;
uint32_t lastReport = 0;
uint32_t toggleCount = 0;

void resetDetection() {
    armed = false;
    pressTiming = false;
    releaseTiming = false;
}

void resetFilter() {
    filterSum = 0;
    filterIndex = 0;
    filterCount = 0;
    for (unsigned i = 0; i < FILTER_SIZE; i++) {
        filterValues[i] = 0;
    }
}

void showLight() {
    const uint32_t colour = lightOn
        ? ring.Color(255, 180, 90)
        : ring.Color(0, 0, 0);

    for (uint16_t i = 0; i < LED_COUNT; i++) {
        ring.setPixelColor(i, colour);
    }
    ring.show();
}

void startZero() {
    collectingZero = true;
    zeroValid = false;
    zeroSum = 0;
    zeroCount = 0;
    zeroStarted = millis();
    resetDetection();
    resetFilter();
    Serial.println("ZERO: Keep the PCB unloaded and still.");
}

void updateButton(long magnitude, uint32_t now) {
    // A lower release threshold provides hysteresis.
    const long releaseThreshold = threshold / 2;

    if (!armed) {
        if (magnitude < releaseThreshold) {
            if (!releaseTiming) {
                releaseTiming = true;
                releaseStarted = now;
            }

            if (now - releaseStarted >= RELEASE_CONFIRM_MS) {
                armed = true;
                releaseTiming = false;
                Serial.println("ARMED: Ready for next press.");
            }
        } else {
            releaseTiming = false;
        }
        return;
    }

    if (magnitude >= threshold) {
        if (!pressTiming) {
            pressTiming = true;
            pressStarted = now;
        }

        if (now - pressStarted >= PRESS_CONFIRM_MS) {
            lightOn = !lightOn;
            toggleCount++;
            showLight();

            Serial.printf(
                "PRESS | Magnitude: %ld | Threshold: %ld"
                " | Light: %s | Count: %lu\n",
                magnitude, threshold, lightOn ? "ON" : "OFF",
                static_cast<unsigned long>(toggleCount)
            );

            resetDetection();
        }
    } else {
        pressTiming = false;
    }
}

void setup() {
    for (uint8_t pin : MOTOR_PINS) {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, LOW);
    }
    pinMode(BACKLIGHT_PIN, OUTPUT);
    digitalWrite(BACKLIGHT_PIN, LOW);

    Serial.begin(115200);
    delay(3000);

    ring.begin();
    ring.setBrightness(12);
    showLight();

    Serial.println("\n=== SmartKnob pressure-button test v1 ===");
    Serial.println("DOUT: 12 | SCK: 13 | LEDs: 36 on GPIO 11");
    Serial.println("Channel A, gain 128. Values are ADC counts.");
    Serial.println("+ raises threshold, - lowers threshold.");
    Serial.println("t captures a new unloaded zero point.");
    Serial.println("Threshold changes are not saved after reset.");
    Serial.println("Send t when the PCB is unloaded.");

    adc.begin(HX_DOUT, HX_SCK, 128);
    adc.power_down();
    delay(1);
    adc.power_up();
    delay(500);
    lastData = millis();
}

void loop() {
    while (Serial.available()) {
        const char command = Serial.read();

        if (command == 't' || command == 'T') {
            startZero();
        } else if (command == '+' || command == '-') {
            threshold += command == '+'
                ? THRESHOLD_STEP : -THRESHOLD_STEP;

            if (threshold < 1000) threshold = 1000;
            if (threshold > 1000000) threshold = 1000000;

            // Require release after changing the threshold.
            resetDetection();
            Serial.printf(
                "Threshold: %ld | Release below: %ld\n",
                threshold, threshold / 2
            );
        }
    }

    const uint32_t now = millis();

    if (collectingZero && now - zeroStarted > 6000) {
        collectingZero = false;
        Serial.println("ZERO FAILED: Not enough data. Send t to retry.");
    }

    if (!adc.is_ready()) {
        if (now - lastData >= 2000 &&
            now - lastWarning >= 2000) {
            Serial.println("TIMEOUT: No ready data. Check ADC, then send t.");
            lastWarning = now;
            zeroValid = false;
            collectingZero = false;
            resetDetection();
            resetFilter();
        }
        delay(1);
        return;
    }

    const long raw = adc.read();
    lastData = millis();

    if (raw <= -8388608L || raw >= 8388607L) {
        zeroValid = false;
        collectingZero = false;
        resetDetection();
        resetFilter();

        if (millis() - lastWarning >= 1000) {
            Serial.println("ADC LIMIT: Switching disabled. Check bridge.");
            lastWarning = millis();
        }
        delay(1);
        return;
    }

    if (collectingZero) {
        zeroSum += raw;
        zeroCount++;

        if (zeroCount >= ZERO_SAMPLES) {
            zeroOffset = static_cast<long>(zeroSum / zeroCount);
            zeroValid = true;
            collectingZero = false;
            resetFilter();
            Serial.printf("ZERO SET: %ld counts\n", zeroOffset);
        }
        delay(1);
        return;
    }

    filterSum -= filterValues[filterIndex];
    filterValues[filterIndex] = raw;
    filterSum += raw;
    filterIndex = (filterIndex + 1) % FILTER_SIZE;

    if (filterCount < FILTER_SIZE) filterCount++;

    const long filtered = static_cast<long>(filterSum / filterCount);
    const long delta = zeroValid ? filtered - zeroOffset : 0;
    const long magnitude = labs(delta);

    if (zeroValid && filterCount == FILTER_SIZE) {
        updateButton(magnitude, millis());
    }

    if (millis() - lastReport >= 250) {
        Serial.printf(
            "Time: %lu s | Raw: %ld | Filtered: %ld",
            static_cast<unsigned long>(millis() / 1000),
            raw, filtered
        );

        if (zeroValid) {
            Serial.printf(" | Delta: %ld | Magnitude: %ld", delta, magnitude);
        } else {
            Serial.print(" | ZERO NOT SET: Send t");
        }

        Serial.printf(
            " | Threshold: %ld | Armed: %s | Light: %s\n",
            threshold, armed ? "YES" : "NO", lightOn ? "ON" : "OFF"
        );
        lastReport = millis();
    }

    delay(1);
}
// Generated with ChatGPT assistance based on my project requirements.
// Initial HX711 diagnostic test. Hardware validation is pending.
// Values are ADC counts, not calibrated force or weight.

#include <Arduino.h>
#include <HX711.h>
#include <stdint.h>

constexpr uint8_t HX_DOUT = 12;
constexpr uint8_t HX_SCK = 13;
constexpr uint8_t MOTOR_PINS[] = {4, 7, 5, 16, 6, 15};
constexpr uint8_t BACKLIGHT_PIN = 9;

constexpr uint16_t ZERO_SAMPLES = 32;
constexpr uint32_t DATA_TIMEOUT_MS = 2000;
constexpr uint32_t ZERO_TIMEOUT_MS = 6000;

HX711 adc;

bool zeroValid = false;
bool collectingZero = false;
int64_t zeroSum = 0;
uint16_t zeroCount = 0;
long zeroOffset = 0;

uint32_t zeroStarted = 0;
uint32_t lastData = 0;
uint32_t lastWarning = 0;
uint32_t lastReport = 0;

int64_t reportSum = 0;
uint32_t reportCount = 0;
long reportMin = 8388607L;
long reportMax = -8388608L;
bool reportAtLimit = false;

void resetReport() {
    reportSum = 0;
    reportCount = 0;
    reportMin = 8388607L;
    reportMax = -8388608L;
    reportAtLimit = false;
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

    Serial.println("\n=== SmartKnob HX711 test v1 ===");
    Serial.println("DOUT: GPIO 12 | SCK: GPIO 13");
    Serial.println("Channel A | Gain: 128");
    Serial.println("Bridge: R3, R4, R5, R6 = 390 ohms each");
    Serial.println("Send t to capture the unloaded zero point.");
    Serial.println("No force calibration. Values are ADC counts.");

    adc.begin(HX_DOUT, HX_SCK, 128);

    // Reset the ADC to a known state.
    adc.power_down();
    delay(1);
    adc.power_up();

    // Allow startup and filter settling.
    delay(500);

    lastData = millis();
    lastReport = millis();
}

void loop() {
    const uint32_t now = millis();

    while (Serial.available()) {
        const char command = Serial.read();
        if (command == 't' || command == 'T') {
            collectingZero = true;
            zeroSum = 0;
            zeroCount = 0;
            zeroStarted = now;
            Serial.println("ZERO: Keep the PCB unloaded and still.");
        }
    }

    if (collectingZero && now - zeroStarted > ZERO_TIMEOUT_MS) {
        collectingZero = false;
        Serial.println("ZERO FAILED: Not enough samples.");
    }

    if (!adc.is_ready()) {
        if (now - lastData >= DATA_TIMEOUT_MS &&
            now - lastWarning >= DATA_TIMEOUT_MS) {
            Serial.println("TIMEOUT: HX711 is not providing ready data.");
            lastWarning = now;
            resetReport();
        }
        delay(1);
        return;
    }

    const long raw = adc.read();
    lastData = millis();

    const bool atLimit =
        raw <= -8388608L || raw >= 8388607L;

    if (collectingZero) {
        if (atLimit) {
            collectingZero = false;
            Serial.println("ZERO FAILED: ADC at measurement limit.");
        } else {
            zeroSum += raw;
            zeroCount++;

            if (zeroCount >= ZERO_SAMPLES) {
                zeroOffset = static_cast<long>(zeroSum / zeroCount);
                zeroValid = true;
                collectingZero = false;
                resetReport();
                Serial.printf("ZERO SET: %ld counts\n", zeroOffset);
            }
        }
    }

    reportSum += raw;
    reportCount++;
    if (raw < reportMin) reportMin = raw;
    if (raw > reportMax) reportMax = raw;
    reportAtLimit = reportAtLimit || atLimit;

    if (millis() - lastReport >= 250) {
        const long average = static_cast<long>(
            reportSum / reportCount
        );

        Serial.printf(
            "Time: %lu s | Raw: %ld | Average: %ld | Range: %ld",
            static_cast<unsigned long>(millis() / 1000),
            raw, average, reportMax - reportMin
        );

        if (zeroValid) {
            Serial.printf(" | Delta: %ld", average - zeroOffset);
        } else {
            Serial.print(" | Delta: ZERO NOT SET");
        }

        Serial.printf(
            " | Samples: %lu | ADC limit: %s\n",
            static_cast<unsigned long>(reportCount),
            reportAtLimit ? "YES" : "NO"
        );

        resetReport();
        lastReport = millis();
    }

    delay(1);
}
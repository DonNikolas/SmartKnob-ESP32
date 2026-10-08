// Generated with ChatGPT assistance based on my project requirements.
// Initial MT6701 angle and magnetic field diagnostic.
// Hardware validation pending. CRC checking is not implemented yet.

#include <Arduino.h>

constexpr uint8_t SENSOR_CS = 14;
constexpr uint8_t SENSOR_CLK = 21;
constexpr uint8_t SENSOR_DATA = 47;

constexpr uint8_t MOTOR_PINS[] = {4, 7, 5, 16, 6, 15};
constexpr uint8_t BACKLIGHT_PIN = 9;

// Read the complete SSI frame using software-generated clock pulses.
// Data changes on rising edges and is sampled on falling edges.
uint32_t readSensorFrame() {
    uint32_t frame = 0;

    // Prepare the clock while the sensor is deselected.
    digitalWrite(SENSOR_CLK, LOW);
    delayMicroseconds(3);

    digitalWrite(SENSOR_CS, LOW);
    delayMicroseconds(3);

    for (unsigned bit = 0; bit < 24; bit++) {
        digitalWrite(SENSOR_CLK, HIGH);
        delayMicroseconds(3);

        digitalWrite(SENSOR_CLK, LOW);
        frame = (frame << 1) | (digitalRead(SENSOR_DATA) ? 1U : 0U);
        delayMicroseconds(3);
    }

    digitalWrite(SENSOR_CS, HIGH);
    digitalWrite(SENSOR_CLK, HIGH);

    return frame;
}

const char* fieldDescription(uint8_t status) {
    switch (status & 0x03) {
        case 0: return "NORMAL";
        case 1: return "TOO STRONG";
        case 2: return "TOO WEAK";
        default: return "RESERVED";
    }
}

void setup() {
    // Keep all motor driver inputs inactive.
    for (uint8_t pin : MOTOR_PINS) {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, LOW);
    }

    pinMode(BACKLIGHT_PIN, OUTPUT);
    digitalWrite(BACKLIGHT_PIN, LOW);

    pinMode(SENSOR_CS, OUTPUT);
    digitalWrite(SENSOR_CS, HIGH);

    pinMode(SENSOR_CLK, OUTPUT);
    digitalWrite(SENSOR_CLK, HIGH);

    pinMode(SENSOR_DATA, INPUT);

    Serial.begin(115200);
    delay(3000);

    Serial.println("\n=== SmartKnob MT6701 angle test ===");
    Serial.println("CS: GPIO 14 | CLK: GPIO 21 | DATA: GPIO 47");
    Serial.println("Motor driver inputs LOW.");
    Serial.println("Turn the knob slowly by hand.");
    Serial.println("CRC is not checked in this initial diagnostic.");
}

void loop() {
    const uint32_t frame = readSensorFrame();

    // Constant all-zero/all-one frames require investigation.
    // This check alone does not validate communication.
    if (frame == 0 || frame == 0xFFFFFFUL) {
        Serial.printf(
            "WARNING: constant-level frame 0x%06lX\n",
            static_cast<unsigned long>(frame)
        );
    } else {
        const uint16_t rawAngle = (frame >> 10) & 0x3FFF;
        const uint8_t status = (frame >> 6) & 0x0F;
        const float angle = rawAngle * (360.0f / 16384.0f);

        Serial.printf(
            "Angle: %7.2f deg | Raw: %5u | Field: %s"
            " | Track: %s | Frame: 0x%06lX\n",
            angle,
            static_cast<unsigned>(rawAngle),
            fieldDescription(status),
            (status & 0x08) ? "LOST" : "OK",
            static_cast<unsigned long>(frame)
        );
    }

    delay(100);
}
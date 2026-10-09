// Generated with ChatGPT assistance based on my project requirements.
// Initial MT6701 angle and magnetic field diagnostic.
// CRC checking and diagnostic counters added; hardware validation pending.

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

// CRC-6: polynomial x^6 + x + 1, initial value 0.
// Process the 18 angle/status bits, most significant bit first.
uint8_t calculateCRC(uint32_t data) {
    uint8_t crc = 0;

    for (int bit = 17; bit >= 0; bit--) {
        const bool feedback =
            ((crc >> 5) ^ (data >> bit)) & 1U;

        crc = (crc << 1) & 0x3F;

        if (feedback) {
            crc ^= 0x03;
        }
    }

    return crc;
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
    Serial.println("CRC checking enabled. Send r to reset counters.");
}

void loop() {
    static uint32_t totalFrames = 0;
    static uint32_t invalidFrames = 0;
    static uint32_t crcErrors = 0;
    static uint32_t weakFrames = 0;
    static uint32_t strongFrames = 0;
    static uint32_t lostFrames = 0;

    // Reset counters without restarting the sensor.
    while (Serial.available() > 0) {
        if (Serial.read() == 'r') {
            totalFrames = 0;
            invalidFrames = 0;
            crcErrors = 0;
            weakFrames = 0;
            strongFrames = 0;
            lostFrames = 0;
            Serial.println("Counters reset.");
        }
    }

    const uint32_t frame = readSensorFrame();
    totalFrames++;

    const uint8_t receivedCRC = frame & 0x3F;
    const uint8_t expectedCRC = calculateCRC(frame >> 6);

    // Reject constant-level frames even if their CRC happens to match.
    if (frame == 0 || frame == 0xFFFFFFUL) {
        invalidFrames++;
        Serial.printf(
            "INVALID FRAME: 0x%06lX\n",
            static_cast<unsigned long>(frame)
        );
    } else if (receivedCRC != expectedCRC) {
        crcErrors++;
        Serial.printf(
            "CRC ERROR: received %02X, expected %02X"
            " | Frame: 0x%06lX\n",
            static_cast<unsigned>(receivedCRC),
            static_cast<unsigned>(expectedCRC),
            static_cast<unsigned long>(frame)
        );
    } else {
        // Interpret angle and status only after a successful CRC check.
        const uint16_t rawAngle = (frame >> 10) & 0x3FFF;
        const uint8_t status = (frame >> 6) & 0x0F;
        const uint8_t field = status & 0x03;
        const bool trackLost = (status & 0x08) != 0;
        const float angle = rawAngle * (360.0f / 16384.0f);

        if (field == 2) weakFrames++;
        if (field == 1) strongFrames++;
        if (trackLost) lostFrames++;

        Serial.printf(
            "Angle: %7.2f deg | CRC: OK | Field: %s"
            " | Track: %s\n",
            angle,
            fieldDescription(status),
            trackLost ? "LOST" : "OK"
        );
    }

    // Counts describe sampled frames, not separate fault events.
    if (totalFrames % 50 == 0) {
        Serial.printf(
            "COUNTERS | Total: %lu | Invalid: %lu | CRC errors: %lu"
            " | Weak: %lu | Strong: %lu | Track lost: %lu\n",
            static_cast<unsigned long>(totalFrames),
            static_cast<unsigned long>(invalidFrames),
            static_cast<unsigned long>(crcErrors),
            static_cast<unsigned long>(weakFrames),
            static_cast<unsigned long>(strongFrames),
            static_cast<unsigned long>(lostFrames)
        );
    }

    delay(100);
}
// Generated with ChatGPT assistance for SmartKnob-NB.
// Sensor dropout diagnostic only. All motor inputs remain LOW.
// Counts describe sampled flags, not the sensor's internal fault duration.
#include <Arduino.h>

constexpr uint8_t MOTOR_PINS[] = {4, 7, 5, 16, 6, 15};
constexpr uint8_t SENSOR_CS = 14, SENSOR_CLK = 21, SENSOR_DATA = 47;
constexpr uint32_t TEST_US = 30000000;

bool running = false;
uint32_t startedUs = 0, lastReadUs = 0;
uint32_t total = 0, valid = 0, weak = 0, lost = 0, crcErrors = 0, invalid = 0;
uint32_t currentLostRun = 0, maxLostRun = 0, lostRuns = 0;
uint32_t currentBadRun = 0, maxBadRun = 0;
uint32_t firstBadUs = 0, maxRecoveryUs = 0, maxSampleGapUs = 0;
uint32_t gapsOver2ms = 0;

uint8_t crc6(uint32_t data) {
    uint8_t crc = 0;
    for (int bit = 17; bit >= 0; --bit) {
        const bool feedback = ((crc >> 5) ^ (data >> bit)) & 1U;
        crc = (crc << 1) & 0x3F;
        if (feedback) crc ^= 0x03;
    }
    return crc;
}

uint32_t readFrame() {
    uint32_t frame = 0;
    digitalWrite(SENSOR_CLK, LOW);
    delayMicroseconds(3);
    digitalWrite(SENSOR_CS, LOW);
    delayMicroseconds(3);
    for (unsigned bit = 0; bit < 24; ++bit) {
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

void report(const char* result, uint32_t now) {
    running = false;
    Serial.printf("\n=== ANGLE DROPOUT TEST: %s ===\n", result);
    Serial.printf("Duration: %lu ms | Samples: %lu\n",
        static_cast<unsigned long>((now - startedUs) / 1000),
        static_cast<unsigned long>(total));
    Serial.printf("Valid frames: %lu | CRC errors: %lu | Invalid frames: %lu\n",
        static_cast<unsigned long>(valid), static_cast<unsigned long>(crcErrors),
        static_cast<unsigned long>(invalid));
    Serial.printf("Weak-field frames: %lu | Track-lost frames: %lu\n",
        static_cast<unsigned long>(weak), static_cast<unsigned long>(lost));
    Serial.printf("Track-lost runs: %lu | Longest run: %lu samples\n",
        static_cast<unsigned long>(lostRuns), static_cast<unsigned long>(maxLostRun));
    Serial.printf("Longest unusable run: %lu samples\n",
        static_cast<unsigned long>(maxBadRun));
    Serial.printf("Longest completed recovery interval: %lu us\n",
        static_cast<unsigned long>(maxRecoveryUs));
    Serial.printf("Maximum sample gap: %lu us | Gaps over 2000 us: %lu\n",
        static_cast<unsigned long>(maxSampleGapUs),
        static_cast<unsigned long>(gapsOver2ms));
    Serial.printf("Unusable run still open at end: %lu samples\n",
        static_cast<unsigned long>(currentBadRun));
    Serial.println("Recovery interval: first unusable sample to next usable sample.");
    Serial.println("Weak-field-only frames count as usable here, not as accuracy verified.");
    Serial.println("Send t for another 30-second test. Motor remains OFF.");
}

void startTest() {
    total = valid = weak = lost = crcErrors = invalid = 0;
    currentLostRun = maxLostRun = lostRuns = 0;
    currentBadRun = maxBadRun = firstBadUs = maxRecoveryUs = 0;
    maxSampleGapUs = gapsOver2ms = lastReadUs = 0;
    Serial.println("START: 30 seconds. No output during sampling. x = cancel.");
    Serial.flush();
    startedUs = micros();
    running = true;
}

void setup() {
    for (uint8_t pin : MOTOR_PINS) { pinMode(pin, OUTPUT); digitalWrite(pin, LOW); }
    pinMode(9, OUTPUT); digitalWrite(9, LOW);
    pinMode(SENSOR_CS, OUTPUT); digitalWrite(SENSOR_CS, HIGH);
    pinMode(SENSOR_CLK, OUTPUT); digitalWrite(SENSOR_CLK, HIGH);
    pinMode(SENSOR_DATA, INPUT);
    Serial.begin(115200);
    delay(3000);
    Serial.println("\n=== SmartKnob angle dropout diagnostic ===");
    Serial.println("Motor inputs LOW. Send t to start, x to cancel.");
}

void loop() {
    if (Serial.available() > 0) {
        const char c = Serial.read();
        if (c == 't' && !running) startTest();
        else if (c == 'x' && running) report("CANCELLED", micros());
    }
    if (!running) { delay(1); return; }
    const uint32_t now = micros();
    if (now - startedUs >= TEST_US) { report("FINISHED", now); return; }
    if (total && now - lastReadUs < 1000) { delayMicroseconds(20); return; }
    if (total) {
        const uint32_t gap = now - lastReadUs;
        if (gap > maxSampleGapUs) maxSampleGapUs = gap;
        if (gap > 2000) ++gapsOver2ms;
    }
    lastReadUs = now;
    ++total;
    const uint32_t frame = readFrame();
    bool trackLost = false, usable = false;
    if (frame == 0 || frame == 0xFFFFFFUL) ++invalid;
    else if (crc6(frame >> 6) != (frame & 63)) ++crcErrors;
    else {
        ++valid;
        const uint8_t status = (frame >> 6) & 15;
        const uint8_t field = status & 3;
        if (field == 2) ++weak;
        trackLost = (status & 8) != 0;
        usable = !trackLost && (field == 0 || field == 2);
    }
    if (trackLost) {
        ++lost;
        if (!currentLostRun) ++lostRuns;
        ++currentLostRun;
        if (currentLostRun > maxLostRun) maxLostRun = currentLostRun;
    } else currentLostRun = 0;
    if (!usable) {
        if (!currentBadRun) firstBadUs = now;
        ++currentBadRun;
        if (currentBadRun > maxBadRun) maxBadRun = currentBadRun;
    } else if (currentBadRun) {
        const uint32_t interval = now - firstBadUs;
        if (interval > maxRecoveryUs) maxRecoveryUs = interval;
        currentBadRun = 0;
    }
}

// Generated with ChatGPT assistance based on my project requirements.
// Tested on my SmartKnob PCB: 8 MiB detected, 1 MiB passed four patterns.

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <esp32s3/spiram.h>

constexpr size_t EXPECTED_PSRAM = 8UL * 1024 * 1024;
constexpr size_t TEST_BYTES = 1UL * 1024 * 1024;
constexpr size_t TEST_WORDS = TEST_BYTES / sizeof(uint32_t);

constexpr uint8_t MOTOR_PINS[] = {4, 7, 5, 16, 6, 15};
constexpr uint8_t BACKLIGHT_PIN = 9;

// Use different values at different memory positions.
uint32_t testPattern(size_t index, uint32_t seed) {
    return (static_cast<uint32_t>(index) * 2654435761UL) ^ seed;
}

void runMemoryTest() {
    Serial.println("\n=== SmartKnob PSRAM test ===");

    const size_t total = esp_spiram_get_size();
    const size_t available =
        heap_caps_get_free_size(MALLOC_CAP_SPIRAM);

    Serial.printf("PSRAM size: %u bytes\n",
                  static_cast<unsigned>(total));
    Serial.printf("PSRAM free: %u bytes\n",
                  static_cast<unsigned>(available));

    if (total == 0) {
        Serial.println("FAIL: No usable PSRAM detected.");
        return;
    }

    const bool sizeCorrect = (total == EXPECTED_PSRAM);
    Serial.println(sizeCorrect
        ? "Size check: PASS (8 MiB)"
        : "Size check: FAIL (expected 8 MiB)");

    // Allocate only from PSRAM, without falling back to internal RAM.
    uint32_t* memory = static_cast<uint32_t*>(
        heap_caps_malloc(
            TEST_BYTES,
            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT
        )
    );

    if (memory == nullptr) {
        Serial.println("FAIL: Could not allocate 1 MiB in PSRAM.");
        return;
    }

    Serial.println("Allocation: PASS (1 MiB)");

    const uint32_t seeds[] = {
        0x00000000UL,
        0xFFFFFFFFUL,
        0xAAAAAAAAUL,
        0x55555555UL
    };

    bool memoryCorrect = true;

    for (unsigned pass = 0; pass < 4; pass++) {
        Serial.printf("Pattern %u/4: writing...\n", pass + 1);

        for (size_t i = 0; i < TEST_WORDS; i++) {
            memory[i] = testPattern(i, seeds[pass]);

            if ((i % 4096) == 0) {
                delay(1);
            }
        }

        // Read back after the complete buffer has been written.
        Serial.println("Reading and checking...");

        for (size_t i = 0; i < TEST_WORDS; i++) {
            const uint32_t expected = testPattern(i, seeds[pass]);
            const uint32_t actual = memory[i];

            if (actual != expected) {
                Serial.printf(
                    "FAIL at byte %u: expected %08lX, read %08lX\n",
                    static_cast<unsigned>(i * sizeof(uint32_t)),
                    static_cast<unsigned long>(expected),
                    static_cast<unsigned long>(actual)
                );
                memoryCorrect = false;
                break;
            }

            if ((i % 4096) == 0) {
                delay(1);
            }
        }

        if (!memoryCorrect) {
            break;
        }

        Serial.printf("Pattern %u/4: PASS\n", pass + 1);
    }

    heap_caps_free(memory);

    Serial.println(sizeCorrect && memoryCorrect
        ? "RESULT: PASS - 8 MiB detected; 1 MiB passed all patterns."
        : "RESULT: FAIL");

    Serial.println("=== Test finished ===");
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

    runMemoryTest();
    Serial.println("Send t to repeat the test.");
}

void loop() {
    if (Serial.available() > 0 && Serial.read() == 't') {
        runMemoryTest();
        Serial.println("Send t to repeat the test.");
    }

    delay(10);
}
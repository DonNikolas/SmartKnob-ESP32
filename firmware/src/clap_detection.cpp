// Generated with ChatGPT assistance based on my project requirements.
// Based on the tested microphone LED level display.
// Clap detection is experimental and requires validation.

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <driver/i2s.h>
#include <esp_err.h>
#include <math.h>

constexpr uint8_t MIC_CLK = 38;
constexpr uint8_t MIC_DATA = 39;
constexpr uint8_t MIC_SELECT = 40;
constexpr uint8_t MOTOR_PINS[] = {4, 7, 5, 16, 6, 15};
constexpr uint8_t BACKLIGHT_PIN = 9;

constexpr uint8_t LED_PIN = 11;
constexpr uint16_t LED_COUNT = 36;
constexpr uint32_t SAMPLE_RATE = 16000;
constexpr size_t FRAMES = 256;
constexpr i2s_port_t MIC_PORT = I2S_NUM_0;

Adafruit_NeoPixel ring(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

int16_t input[FRAMES * 2];
size_t filledBytes = 0;

// Digital RMS level that fills the complete ring.
// This affects the display scale, not microphone gain.
float fullScale = 3200.0f;
float shownLevel = 0.0f;
float reportPeak[2] = {};
uint32_t peakTime = 0;

void checkResult(esp_err_t result, const char* step) {
    if (result == ESP_OK) return;

    ring.clear();
    ring.show();
    Serial.printf("FAIL: %s: %s\n", step, esp_err_to_name(result));
    while (true) delay(1000);
}

float slotRms(unsigned slot) {
    double sum = 0;
    double squares = 0;

    for (size_t i = 0; i < FRAMES; i++) {
        const double value = input[2 * i + slot];
        sum += value;
        squares += value * value;
    }

    const double mean = sum / FRAMES;
    const double variance = squares / FRAMES - mean * mean;
    return static_cast<float>(sqrt(variance > 0 ? variance : 0));
}

void drawLevel(float level) {
    float fraction = level / fullScale;
    if (fraction > 1.0f) fraction = 1.0f;
    if (fraction < 0.0f) fraction = 0.0f;

    const uint16_t lit = static_cast<uint16_t>(
        fraction * LED_COUNT + 0.5f
    );

    ring.clear();
    for (uint16_t i = 0; i < lit; i++) {
        uint32_t colour;
        if (i < 24) {
            colour = ring.Color(0, 255, 0);
        } else if (i < 32) {
            colour = ring.Color(255, 140, 0);
        } else {
            colour = ring.Color(255, 0, 0);
        }
        ring.setPixelColor(i, colour);
    }
    ring.show();
}


// Initial tuning values; these must be verified on the PCB.
constexpr float CLAP_MIN_RMS = 600.0f;
constexpr float CLAP_BACKGROUND_FACTOR = 4.0f;
constexpr float CLAP_RISE_FACTOR = 1.8f;

constexpr uint32_t CLAP_MAX_DURATION_MS = 160;
constexpr uint32_t CLAP_COOLDOWN_MS = 60;
constexpr uint32_t CLAP_FLASH_MS = 200;

uint32_t clapCount = 0;
uint32_t lastClapTime = 0;


// Initial double-clap timing limits.
constexpr uint32_t DOUBLE_CLAP_MIN_MS = 120;
constexpr uint32_t DOUBLE_CLAP_MAX_MS = 550;
constexpr uint32_t DOUBLE_CLAP_LOCKOUT_MS = 1000;

bool lightOn = false;
bool waitingForSecondClap = false;
bool doubleClapLockout = false;

uint32_t firstClapStart = 0;
uint32_t lastDoubleClapTime = 0;
uint32_t doubleClapCount = 0;

void updateDoubleClapTimeout(uint32_t now) {
    if (doubleClapLockout &&
        now - lastDoubleClapTime >= DOUBLE_CLAP_LOCKOUT_MS) {
        doubleClapLockout = false;
    }

    // Allow time for a second candidate to finish its decay check.
    if (waitingForSecondClap &&
        now - firstClapStart >
            DOUBLE_CLAP_MAX_MS + CLAP_MAX_DURATION_MS) {
        waitingForSecondClap = false;
        Serial.println("PAIR EXPIRED | No second clap.");
    }
}

void registerClap(uint32_t onset, uint32_t duration,
                  float peak, uint32_t now) {
    Serial.printf(
        "CLAP | Start: %lu ms | Duration: %lu ms | Peak RMS: %.1f\n",
        static_cast<unsigned long>(onset),
        static_cast<unsigned long>(duration),
        peak
    );

    if (doubleClapLockout) {
        Serial.println("IGNORED | Double-clap lockout.");
        return;
    }

    if (!waitingForSecondClap) {
        firstClapStart = onset;
        waitingForSecondClap = true;
        Serial.println("FIRST CLAP | Waiting for second clap.");
        return;
    }

    const uint32_t gap = onset - firstClapStart;

    if (gap < DOUBLE_CLAP_MIN_MS) {
        // Reject the pair instead of counting a possible echo.
        waitingForSecondClap = false;
        Serial.printf(
            "PAIR REJECTED | Too fast: %lu ms\n",
            static_cast<unsigned long>(gap)
        );
        return;
    }

    if (gap > DOUBLE_CLAP_MAX_MS) {
        // This impulse may be the first clap of a new pair.
        firstClapStart = onset;
        Serial.printf(
            "NEW FIRST CLAP | Previous gap too long: %lu ms\n",
            static_cast<unsigned long>(gap)
        );
        return;
    }

    waitingForSecondClap = false;
    doubleClapLockout = true;
    lastDoubleClapTime = now;
    doubleClapCount++;
    lightOn = !lightOn;

    Serial.printf(
        "DOUBLE CLAP | Gap: %lu ms | Light: %s | Count: %lu\n",
        static_cast<unsigned long>(gap),
        lightOn ? "ON" : "OFF",
        static_cast<unsigned long>(doubleClapCount)
    );
}



void updateClapDetector(float level, uint32_t now) {
    enum class State { Calibrating, Ready, Candidate, Recovery };

    static State state = State::Calibrating;
    static uint32_t calibrationFrames = 0;
    static double calibrationSum = 0;
    static float background = 0;
    static float previousLevel = 0;
    static float candidatePeak = 0;
    static float releaseLevel = 0;
    static uint32_t candidateStart = 0;
    static uint32_t recoveryStart = 0;
    static unsigned lowFrames = 0;

    // Learn the background from two seconds of audio.
    if (state == State::Calibrating) {
        calibrationSum += level;
        calibrationFrames++;

        if (calibrationFrames >= (2 * SAMPLE_RATE / FRAMES)) {
            background = static_cast<float>(
                calibrationSum / calibrationFrames
            );
            state = State::Ready;
            Serial.printf(
                "READY | Background RMS: %.1f | Trigger RMS: %.1f\n",
                background,
                fmaxf(CLAP_MIN_RMS,
                      background * CLAP_BACKGROUND_FACTOR)
            );
        }

        previousLevel = level;
        return;
    }

    const float trigger = fmaxf(
        CLAP_MIN_RMS, background * CLAP_BACKGROUND_FACTOR
    );

    if (state == State::Ready) {
        const bool fastRise =
            level >= fmaxf(previousLevel, 1.0f) * CLAP_RISE_FACTOR;

        if (level >= trigger && fastRise) {
            candidateStart = now;
            candidatePeak = level;
            lowFrames = 0;
            state = State::Candidate;

            Serial.printf(
                "IMPULSE | RMS: %.1f | Trigger: %.1f\n",
                level, trigger
            );
        } else if (level < trigger) {
            // Follow ordinary background changes gradually.
            background += 0.01f * (level - background);
        }
    } else if (state == State::Candidate) {
        candidatePeak = fmaxf(candidatePeak, level);
        releaseLevel = fmaxf(
            background * 1.8f, candidatePeak * 0.35f
        );

        // Require two consecutive quiet windows.
        if (level <= releaseLevel) {
            lowFrames++;
        } else {
            lowFrames = 0;
        }

        const uint32_t duration = now - candidateStart;

        if (duration > CLAP_MAX_DURATION_MS) {
            waitingForSecondClap = false;
            recoveryStart = now;
            lowFrames = 0;
            state = State::Recovery;
        } else if (lowFrames >= 2) {
            registerClap(candidateStart, duration, candidatePeak, now);

            Serial.printf(
                "CLAP DETECTED | Count: %lu | Peak RMS: %.1f"
                " | Duration: %lu ms\n",
                static_cast<unsigned long>(clapCount),
                candidatePeak,
                static_cast<unsigned long>(duration)
            );

            recoveryStart = now;
            lowFrames = 0;
            state = State::Recovery;
        }
    } else if (state == State::Recovery) {
        // Wait for both the cooldown and a return to a quiet level.
        const float quietLimit = fmaxf(
            CLAP_MIN_RMS * 0.5f, background * 1.8f
        );

        if (level < quietLimit) {
            if (lowFrames < 3) lowFrames++;
        } else {
            lowFrames = 0;
        }

        if (now - recoveryStart >= CLAP_COOLDOWN_MS &&
            lowFrames >= 3) {
            state = State::Ready;
        }
    }

    previousLevel = level;
}


void setup() {
    for (uint8_t pin : MOTOR_PINS) {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, LOW);
    }
    pinMode(BACKLIGHT_PIN, OUTPUT);
    digitalWrite(BACKLIGHT_PIN, LOW);

    pinMode(MIC_SELECT, OUTPUT);
    digitalWrite(MIC_SELECT, LOW);

    Serial.begin(115200);
    delay(3000);

    ring.begin();
    ring.setBrightness(12);
    ring.clear();
    ring.show();

    Serial.println("\n=== SmartKnob clap detection test v1 ===");
    Serial.println("Keep quiet until READY appears.");

    i2s_config_t config = {};
    config.mode = static_cast<i2s_mode_t>(
        I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM
    );
    config.sample_rate = SAMPLE_RATE;
    config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
    config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
    config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    config.dma_buf_count = 8;
    config.dma_buf_len = FRAMES;
    config.use_apll = false;

    checkResult(
        i2s_driver_install(MIC_PORT, &config, 0, nullptr),
        "Install PDM driver"
    );

    i2s_pin_config_t pins = {};
    pins.mck_io_num = I2S_PIN_NO_CHANGE;
    pins.bck_io_num = I2S_PIN_NO_CHANGE;
    pins.ws_io_num = MIC_CLK;
    pins.data_out_num = I2S_PIN_NO_CHANGE;
    pins.data_in_num = MIC_DATA;

    checkResult(i2s_set_pin(MIC_PORT, &pins), "Configure pins");
    checkResult(
        i2s_set_pdm_rx_down_sample(MIC_PORT, I2S_PDM_DSR_16S),
        "Configure PDM conversion"
    );
}

void loop() {
    while (Serial.available()) {
        const char command = Serial.read();
        if (command == '+') {
            fullScale = fmaxf(1.0f, fullScale / 2.0f);
        } else if (command == '-') {
            fullScale = fminf(32768.0f, fullScale * 2.0f);
        } else {
            continue;
        }
        Serial.printf("Full-ring RMS level: %.1f\n", fullScale);
    }

    size_t bytesRead = 0;
    uint8_t* bytes = reinterpret_cast<uint8_t*>(input);
    const esp_err_t result = i2s_read(
        MIC_PORT, bytes + filledBytes, sizeof(input) - filledBytes,
        &bytesRead, pdMS_TO_TICKS(1000)
    );

    if (result != ESP_OK || bytesRead == 0) {
        ring.clear();
        ring.show();
        Serial.printf("READ ERROR: %s\n", esp_err_to_name(result));
        filledBytes = 0;
        shownLevel = 0;
        reportPeak[0] = reportPeak[1] = 0;
        delay(100);
        return;
    }

    filledBytes += bytesRead;
    if (filledBytes < sizeof(input)) return;
    filledBytes = 0;

    static uint32_t warmupStarted = millis();
    if (millis() - warmupStarted < 300) return;

    const float a = slotRms(0);
    const float b = slotRms(1);
    const float level = fmaxf(a, b);
    const uint32_t now = millis();
    updateDoubleClapTimeout(now);
    updateClapDetector(level, now);

    reportPeak[0] = fmaxf(reportPeak[0], a);
    reportPeak[1] = fmaxf(reportPeak[1], b);

    // Fast rise, short peak hold and gradual fall.
    if (level >= shownLevel) {
        shownLevel = level;
        peakTime = now;
    } else if (now - peakTime > 150) {
        shownLevel = fmaxf(level, shownLevel * 0.92f);
    }

    static uint32_t lastDraw = 0;
        if (now - lastDraw >= 30) {
            const uint32_t colour = lightOn
            ? ring.Color(255, 180, 90)
            : ring.Color(0, 0, 0);

            for (uint16_t i = 0; i < LED_COUNT; i++) {
                ring.setPixelColor(i, colour);
            }
            ring.show();
            lastDraw = now;
        }

    static uint32_t lastReport = 0;
    if (now - lastReport >= 250) {
        Serial.printf(
            "Time: %lu s | A RMSmax: %.1f | B RMSmax: %.1f"
            " | Full ring: %.1f\n",
            static_cast<unsigned long>(now / 1000),
            reportPeak[0], reportPeak[1], fullScale
        );
        reportPeak[0] = reportPeak[1] = 0;
        lastReport = now;
    }
}
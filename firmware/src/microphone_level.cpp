// Generated with ChatGPT assistance based on my project requirements.
// Diagnostic level meter. Acoustic response is not yet verified.

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

    Serial.println("\n=== SmartKnob microphone LED level test v1 ===");
    Serial.println("36 LEDs on GPIO 11. PDM clock target: 2.048 MHz.");
    Serial.println("Relative digital level, not calibrated dB SPL.");
    Serial.println("+ = more sensitive, - = less sensitive.");
    Serial.println("Red means display full scale, not ADC clipping.");

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
        drawLevel(shownLevel);
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
// Generated with ChatGPT assistance based on my project requirements.
// Initial hardware test: sample data received.
// Acoustic response and clap detection are not yet verified.

#include <Arduino.h>
#include <driver/i2s.h>
#include <esp_err.h>
#include <math.h>

constexpr uint8_t MIC_CLK = 38;
constexpr uint8_t MIC_DATA = 39;
constexpr uint8_t MIC_SELECT = 40;

constexpr uint8_t MOTOR_PINS[] = {4, 7, 5, 16, 6, 15};
constexpr uint8_t BACKLIGHT_PIN = 9;

constexpr i2s_port_t MIC_PORT = I2S_NUM_0;
constexpr uint32_t SAMPLE_RATE = 16000;
constexpr size_t BUFFER_SAMPLES = 512;

int16_t samples[BUFFER_SAMPLES];

double sampleSum = 0;
double squareSum = 0;
uint32_t sampleCount = 0;
uint32_t clippedCount = 0;
int32_t minimum = 32767;
int32_t maximum = -32768;

void resetStatistics() {
    sampleSum = 0;
    squareSum = 0;
    sampleCount = 0;
    clippedCount = 0;
    minimum = 32767;
    maximum = -32768;
}

void checkResult(esp_err_t result, const char* step) {
    if (result == ESP_OK) {
        return;
    }

    Serial.printf("FAIL: %s: %s\n", step, esp_err_to_name(result));

    while (true) {
        delay(1000);
    }
}

void setup() {
    for (uint8_t pin : MOTOR_PINS) {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, LOW);
    }

    pinMode(BACKLIGHT_PIN, OUTPUT);
    digitalWrite(BACKLIGHT_PIN, LOW);

    // Select the microphone's left channel.
    pinMode(MIC_SELECT, OUTPUT);
    digitalWrite(MIC_SELECT, LOW);

    Serial.begin(115200);
    delay(3000);

    Serial.println("\n=== SmartKnob microphone test ===");
    Serial.println("Microphone: ICS-41350");
    Serial.println("CLK: GPIO 38 | DATA: GPIO 39 | SELECT: GPIO 40 LOW");

    i2s_config_t config = {};
    config.mode = static_cast<i2s_mode_t>(
        I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM
    );
    config.sample_rate = SAMPLE_RATE;
    config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
    config.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
    config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    config.intr_alloc_flags = 0;
    config.dma_buf_count = 8;
    config.dma_buf_len = BUFFER_SAMPLES;
    config.use_apll = false;

    checkResult(
        i2s_driver_install(MIC_PORT, &config, 0, nullptr),
        "Install PDM driver"
    );

    // PDM clock is routed through ws_io_num in this driver.
    i2s_pin_config_t pins = {};
    pins.mck_io_num = I2S_PIN_NO_CHANGE;
    pins.bck_io_num = I2S_PIN_NO_CHANGE;
    pins.ws_io_num = MIC_CLK;
    pins.data_out_num = I2S_PIN_NO_CHANGE;
    pins.data_in_num = MIC_DATA;

    checkResult(
        i2s_set_pin(MIC_PORT, &pins),
        "Configure microphone pins"
    );

    // 64 PDM clock cycles per PCM sample: approximately 1.024 MHz.
    checkResult(
        i2s_set_pdm_rx_down_sample(MIC_PORT, I2S_PDM_DSR_8S),
        "Configure PDM conversion"
    );

    Serial.println("Driver ready: 16000 samples/s, 16-bit mono.");
    Serial.println("Compare a quiet room with speaking near the microphone.");
    Serial.println("RMS and peak-to-peak are digital values, not calibrated dB SPL.");
    Serial.println("Receiving data alone does not prove microphone operation.");
}

void loop() {
    size_t bytesRead = 0;

    const esp_err_t result = i2s_read(
        MIC_PORT,
        samples,
        sizeof(samples),
        &bytesRead,
        pdMS_TO_TICKS(1000)
    );

    if (result != ESP_OK || bytesRead == 0) {
        Serial.printf("READ ERROR: %s | Bytes: %u\n",
                      esp_err_to_name(result),
                      static_cast<unsigned>(bytesRead));
        resetStatistics();
        delay(100);
        return;
    }

    // Ignore the initial settling period while continuing to read data.
    static bool warmedUp = false;
    static uint32_t warmupStarted = millis();

    if (!warmedUp) {
        warmedUp = millis() - warmupStarted >= 300;
        return;
    }

    const size_t count = bytesRead / sizeof(int16_t);

    for (size_t i = 0; i < count; i++) {
        const int32_t value = samples[i];

        sampleSum += value;
        squareSum += static_cast<double>(value) * value;

        if (value < minimum) minimum = value;
        if (value > maximum) maximum = value;

        if (value == -32768 || value == 32767) {
            clippedCount++;
        }

        sampleCount++;
    }

    // Report roughly every half second.
    if (sampleCount >= SAMPLE_RATE / 2) {
        const double mean = sampleSum / sampleCount;
        const double variance = squareSum / sampleCount - mean * mean;
        const double rms = sqrt(variance > 0 ? variance : 0);

        Serial.printf(
            "Time: %lu s | RMS: %.1f | P-P: %ld | "
            "Mean: %.1f | Clipped: %lu | Samples: %lu\n",
            millis() / 1000UL,
            rms,
            static_cast<long>(maximum - minimum),
            mean,
            static_cast<unsigned long>(clippedCount),
            static_cast<unsigned long>(sampleCount)
        );

        resetStatistics();
    }
}
// Generated with ChatGPT assistance based on my project requirements.
// Initial hardware test: sample data received.
// Acoustic response and clap detection are not yet verified.

#include <Arduino.h>
#include <driver/i2s.h>
#include <esp_err.h>
#include <math.h>
#include <arduinoFFT.h>

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

    // Select the microphone's right channel.
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
    config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
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

    // 128 PDM clock cycles per PCM sample: approximately 2.048 MHz.
    checkResult(
        i2s_set_pdm_rx_down_sample(MIC_PORT, I2S_PDM_DSR_16S),
        "Configure PDM conversion"
    );

    Serial.println("Diagnostic v3: FFT bands on both receive slots.");
    Serial.println("Bands: LOW 125-1000 Hz | MID 1000-4000 Hz | HIGH 4000-8000 Hz.");
    Serial.println("Values are relative spectral levels, not calibrated dB.");
}

void loop() {
    constexpr uint16_t FFT_SIZE = 256;

    static int16_t input[FFT_SIZE * 2];
    static float real[FFT_SIZE];
    static float imag[FFT_SIZE];

    static ArduinoFFT<float> fft(
        real, imag, FFT_SIZE, static_cast<float>(SAMPLE_RATE)
    );

    static float bandMax[2][3] = {};
    static size_t filledBytes = 0;
    static uint32_t reportFrames = 0;
    static uint32_t warmupStarted = millis();

    size_t bytesRead = 0;
    uint8_t* buffer = reinterpret_cast<uint8_t*>(input);

    const esp_err_t result = i2s_read(
        MIC_PORT,
        buffer + filledBytes,
        sizeof(input) - filledBytes,
        &bytesRead,
        pdMS_TO_TICKS(1000)
    );

    if (result != ESP_OK || bytesRead == 0) {
        Serial.printf(
            "READ ERROR: %s | Bytes: %u\n",
            esp_err_to_name(result),
            static_cast<unsigned>(bytesRead)
        );
        filledBytes = 0;
        delay(100);
        return;
    }

    // Collect a complete block, including partial driver reads.
    filledBytes += bytesRead;
    if (filledBytes < sizeof(input)) {
        return;
    }
    filledBytes = 0;

    // Discard startup samples while the filters settle.
    if (millis() - warmupStarted < 300) {
        return;
    }

    for (unsigned slot = 0; slot < 2; slot++) {
        for (uint16_t i = 0; i < FFT_SIZE; i++) {
            real[i] = static_cast<float>(input[2 * i + slot]);
            imag[i] = 0;
        }

        fft.dcRemoval();
        fft.windowing(FFTWindow::Hamming, FFTDirection::Forward);
        fft.compute(FFTDirection::Forward);

        float bandPower[3] = {};

        // Sum spectral power within each band.
        // Exclude DC, frequencies below 125 Hz and the Nyquist bin.
        for (uint16_t bin = 1; bin < FFT_SIZE / 2; bin++) {
            const float frequency =
                static_cast<float>(bin) * SAMPLE_RATE / FFT_SIZE;

            if (frequency < 125.0f) {
                continue;
            }

            unsigned band;
            if (frequency < 2000.0f) {
                band = 0;
            } else if (frequency < 6000.0f) {
                band = 1;
            } else {
                band = 2;
            }

            bandPower[band] +=
                real[bin] * real[bin] + imag[bin] * imag[bin];
        }

        for (unsigned band = 0; band < 3; band++) {
            // Relative level with fixed scaling for all reports.
            const float level = sqrtf(bandPower[band]) / FFT_SIZE;

            if (level > bandMax[slot][band]) {
                bandMax[slot][band] = level;
            }
        }
    }

    reportFrames += FFT_SIZE;

    // Hold each band's maximum over approximately half a second.
    if (reportFrames >= SAMPLE_RATE / 2) {
        Serial.printf(
            "Time: %lu s | A low/mid/high: %.2f %.2f %.2f"
            " | B low/mid/high: %.2f %.2f %.2f\n",
            static_cast<unsigned long>(millis() / 1000),
            bandMax[0][0], bandMax[0][1], bandMax[0][2],
            bandMax[1][0], bandMax[1][1], bandMax[1][2]
        );

        for (unsigned slot = 0; slot < 2; slot++) {
            for (unsigned band = 0; band < 3; band++) {
                bandMax[slot][band] = 0;
            }
        }
        reportFrames = 0;
    }
}
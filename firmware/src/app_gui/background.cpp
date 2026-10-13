// Integrated with ChatGPT assistance from bme280_test.cpp and clap_detection.cpp.
// Neither worker writes motor outputs, LVGL objects, LEDs or the serial terminal.
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_BME280.h>
#include <driver/i2s.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <atomic>
#include <math.h>
#include "background.h"
#include "clap_detector.h"
namespace KnobBackground {
namespace {
QueueHandle_t environmentQueue = nullptr, audioQueue = nullptr;
std::atomic<uint32_t> clickEpoch{0};
void environmentTask(void*) {
    Environment value;
    Adafruit_BME280 bme;
    bool wireReady = Wire.begin(48, 41, 100000);
    Wire.setTimeOut(50);
    bool initialized = false;
    for (;;) {
        if (!initialized && wireReady) {
            initialized = bme.begin(0x76, &Wire) && bme.sensorID() == 0x60;
            if (initialized) bme.setSampling(Adafruit_BME280::MODE_FORCED,
                Adafruit_BME280::SAMPLING_X1, Adafruit_BME280::SAMPLING_X1,
                Adafruit_BME280::SAMPLING_X1, Adafruit_BME280::FILTER_OFF);
        }
        value.valid = false;
        value.status = "BME280 unavailable";
        if (initialized && bme.takeForcedMeasurement()) {
            value.temperature = bme.readTemperature();
            value.humidity = bme.readHumidity();
            value.pressure = bme.readPressure() / 100.0f;
            value.valid = isfinite(value.temperature) && isfinite(value.humidity) &&
                isfinite(value.pressure) && value.humidity >= 0 && value.humidity <= 100 && value.pressure > 0;
            value.status = value.valid ? "Live / 2 seconds" : "Invalid measurement";
            if (value.valid) value.measuredAt = millis();
        }
        if (!value.valid) initialized = false;
        ++value.sequence;
        xQueueOverwrite(environmentQueue, &value);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
void audioTask(void*) {
    constexpr unsigned FRAMES = 256;
    constexpr i2s_port_t port = I2S_NUM_0;
    Audio value;
    auto report = [&](const char* status) { value.status = status; xQueueOverwrite(audioQueue, &value); };
    pinMode(40, OUTPUT); digitalWrite(40, LOW);
    i2s_config_t config = {};
    config.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM);
    config.sample_rate = 16000;
    config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
    config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
    config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    config.dma_buf_count = 8; config.dma_buf_len = FRAMES; config.use_apll = false;
    QueueHandle_t events = nullptr;
    if (i2s_driver_install(port, &config, 16, &events) != ESP_OK) {
        report("Microphone init failed"); vTaskDelete(nullptr); return;
    }
    i2s_pin_config_t pins = {};
    pins.mck_io_num = pins.bck_io_num = pins.data_out_num = I2S_PIN_NO_CHANGE;
    pins.ws_io_num = 38; pins.data_in_num = 39;
    if (i2s_set_pin(port, &pins) != ESP_OK ||
        i2s_set_pdm_rx_down_sample(port, I2S_PDM_DSR_16S) != ESP_OK) {
        report("Microphone config failed"); i2s_driver_uninstall(port); vTaskDelete(nullptr); return;
    }
    int16_t input[FRAMES * 2]; size_t filled = 0;
    KnobAudio::Detector detector;
    uint32_t warmup = millis(), lastBlock = warmup, audioTime = warmup;
    uint32_t seenClick = clickEpoch.load(), quietAt = 0;
    bool muted = false;
    report("Clap: learning noise");
    for (;;) {
        bool overflow = false; i2s_event_t event;
        while (xQueueReceive(events, &event, 0) == pdTRUE)
            if (event.type == I2S_EVENT_RX_Q_OVF || event.type == I2S_EVENT_DMA_ERROR) overflow = true;
        size_t received = 0;
        esp_err_t result = i2s_read(port, reinterpret_cast<uint8_t*>(input) + filled,
            sizeof(input) - filled, &received, pdMS_TO_TICKS(40));
        uint32_t now = millis();
        if (result != ESP_OK || overflow || now - lastBlock > 100) {
            filled = 0; detector = KnobAudio::Detector(); warmup = now; lastBlock = audioTime = now;
            report("Clap: audio recovering");
            // Discard queued data so gaps cannot join two unrelated impulses.
            size_t discarded = 0;
            do { i2s_read(port, input, sizeof(input), &discarded, 0); } while (discarded);
            vTaskDelay(pdMS_TO_TICKS(1)); continue;
        }
        filled += received;
        if (filled < sizeof(input)) { vTaskDelay(pdMS_TO_TICKS(1)); continue; }
        filled = 0; lastBlock = now; audioTime += 16;
        if (now - warmup < 300) { audioTime = now; continue; }
        uint32_t epoch = clickEpoch.load();
        if (epoch != seenClick) { seenClick = epoch; muted = true; quietAt = now; }
        if (muted && now - quietAt < 250) { detector.cancel(audioTime); continue; }
        muted = false;
        float level = 0;
        for (unsigned slot = 0; slot < 2; ++slot) {
            double sum = 0, squares = 0;
            for (unsigned i = 0; i < FRAMES; ++i) {
                double sample = input[2 * i + slot]; sum += sample; squares += sample * sample;
            }
            double mean = sum / FRAMES, variance = squares / FRAMES - mean * mean;
            level = fmaxf(level, sqrt(variance > 0 ? variance : 0));
        }
        if (detector.update(level, audioTime)) { ++value.pairs; value.lastPairAt = now; }
        report(detector.ready() ? "Clap: ready" : "Clap: learning noise");
    }
}
}
void suppressClick() { clickEpoch.fetch_add(1, std::memory_order_relaxed); }
void begin() {
    environmentQueue = xQueueCreate(1, sizeof(Environment));
    audioQueue = xQueueCreate(1, sizeof(Audio));
    if (environmentQueue && xTaskCreatePinnedToCore(environmentTask, "bme280", 4096, nullptr, 1, nullptr, 0) != pdPASS) {
        Environment e; e.status = "Sensor task failed"; xQueueOverwrite(environmentQueue, &e);
    }
    if (audioQueue && xTaskCreatePinnedToCore(audioTask, "claps", 6144, nullptr, 2, nullptr, 0) != pdPASS) {
        Audio a; a.status = "Audio task failed"; xQueueOverwrite(audioQueue, &a);
    }
}
void read(Environment& environment, Audio& audio) {
    if (environmentQueue) xQueueReceive(environmentQueue, &environment, 0);
    else environment.status = "Sensor queue failed";
    if (audioQueue) xQueueReceive(audioQueue, &audio, 0);
    else audio.status = "Audio queue failed";
}
}

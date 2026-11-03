// MQTT_PREP_V1
// FFT clap capture on core 0. Motor/control remain on core 1.
// Environmental sensor worker retained from the project's background.cpp.
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_BME280.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <math.h>
#include <atomic>
#include <driver/i2s.h>
#include "clap_stream.h"
#include "background.h"
#include "mqtt_link.h"
namespace KnobBackground {
namespace {
QueueHandle_t environmentQueue = nullptr, audioQueue = nullptr, i2sEvents = nullptr;
constexpr i2s_port_t AUDIO_PORT = I2S_NUM_0;
constexpr uint32_t CLICK_MUTE_MS = 300;
std::atomic<uint32_t> suppressionEpoch{0}, suppressionAt{0};
// Large buffers live in static RAM, not on the audio task stack.
int16_t stereo[ClapFFT::N * 2], mono[ClapFFT::N];
ClapFFT::Analyzer analyzer;
KnobClap::Stream stream;
const char* audioStatus(KnobClap::State state) {
    switch (state) {
        case KnobClap::State::Ready: return "Clap: ready";
        case KnobClap::State::Muted: return "Clap: haptic pause";
        case KnobClap::State::NoSignal: return "Mic: no signal";
        case KnobClap::State::Warming: return "Mic: starting";
        default: return "Mic: calibrating";
    }
}
void audioTask(void*) {
    Audio value;
    i2s_config_t c = {};
    c.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM);
    c.sample_rate = 16000; c.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
    c.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
    c.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    c.dma_buf_count = 8; c.dma_buf_len = ClapFFT::N; c.use_apll = false;
    esp_err_t err = i2s_driver_install(AUDIO_PORT, &c, 16, &i2sEvents);
    const bool installed = err == ESP_OK;
    i2s_pin_config_t pins = {};
    pins.mck_io_num = I2S_PIN_NO_CHANGE; pins.bck_io_num = I2S_PIN_NO_CHANGE;
    pins.ws_io_num = 38; pins.data_out_num = I2S_PIN_NO_CHANGE; pins.data_in_num = 39;
    if (err == ESP_OK) err = i2s_set_pin(AUDIO_PORT, &pins);
    if (err == ESP_OK) err = i2s_set_pdm_rx_down_sample(AUDIO_PORT, I2S_PDM_DSR_16S);
    if (err != ESP_OK) {
        value.status = "Mic: init failed"; ++value.errors;
        xQueueOverwrite(audioQueue, &value);
        if (installed) i2s_driver_uninstall(AUDIO_PORT);
        vTaskDelete(nullptr); return;
    }
    size_t filled = 0;
    uint32_t lastFrameAt = millis(), epoch = suppressionEpoch.load();
    unsigned discard = 0;
    for (;;) {
        const uint32_t beforeRead = millis();
        size_t got = 0;
        const esp_err_t result = i2s_read(AUDIO_PORT,
            reinterpret_cast<uint8_t*>(stereo) + filled, sizeof(stereo) - filled,
            &got, pdMS_TO_TICKS(100));
        const uint32_t now = millis();
        bool broken = result != ESP_OK || got == 0 || got % 4 != 0 ||
                      now - lastFrameAt > 64 || now - beforeRead > 64;
        i2s_event_t event;
        while (xQueueReceive(i2sEvents, &event, 0) == pdTRUE)
            if (event.type == I2S_EVENT_DMA_ERROR || event.type == I2S_EVENT_RX_Q_OVF) broken = true;
        if (broken) {
            ++value.errors; filled = 0; discard = 8; lastFrameAt = now;
            stream.reset(); analyzer.reset(); value.status = "Mic: resync";
            xQueueOverwrite(audioQueue, &value);
            vTaskDelay(pdMS_TO_TICKS(1)); continue;
        }
        filled += got;
        if (filled < sizeof(stereo)) continue;
        filled = 0; lastFrameAt = now;
        if (discard) { --discard; continue; } // Drain possible buffered pre-gap data.
        const uint32_t nextEpoch = suppressionEpoch.load();
        if (nextEpoch != epoch) { epoch = nextEpoch; stream.cancelPair(); }
        for (unsigned i = 0; i < ClapFFT::N; ++i) mono[i] = stereo[2 * i]; // Tested slot A.
        const auto features = analyzer.analyze(mono);
        const auto detection = stream.update(features, clickSuppressed(now));
        value.rms = features.rms; value.measuredAt = now; ++value.frames;
        value.status = audioStatus(stream.state);
        // A haptic may start during FFT computation. Do not publish that pair.
        if (suppressionEpoch.load() != epoch || clickSuppressed(millis())) stream.cancelPair();
        else {
            if (detection.event == 1) ++value.candidates;
            if (detection.event == 2) { ++value.pairs; value.lastPairAt = millis(); }
        }
        xQueueOverwrite(audioQueue, &value);
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}
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
        KnobMQTT::temperature(value.temperature, value.valid, value.measuredAt);
        ++value.sequence;
        xQueueOverwrite(environmentQueue, &value);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
}
void suppressClick() {
    suppressionAt.store(millis()); suppressionEpoch.fetch_add(1);
}
bool clickSuppressed(uint32_t now) {
    return suppressionEpoch.load() != 0 && now - suppressionAt.load() < CLICK_MUTE_MS;
}
void begin() {
    KnobMQTT::begin();
    // Initialize SELECT LOW and clock before handing capture to the audio worker.
    pinMode(38, OUTPUT); digitalWrite(38, LOW);
    pinMode(40, OUTPUT); digitalWrite(40, LOW);
    pinMode(39, INPUT);
    audioQueue = xQueueCreate(1, sizeof(Audio));
    if (audioQueue && xTaskCreatePinnedToCore(audioTask, "clap_fft", 8192, nullptr, 2, nullptr, 0) != pdPASS) {
        Audio a; a.status = "Mic: task failed"; xQueueOverwrite(audioQueue, &a);
    }
    environmentQueue = xQueueCreate(1, sizeof(Environment));
    if (environmentQueue && xTaskCreatePinnedToCore(environmentTask, "bme280", 4096, nullptr, 1, nullptr, 0) != pdPASS) {
        Environment e; e.status = "Sensor task failed"; xQueueOverwrite(environmentQueue, &e);
    }
}
void read(Environment& environment, Audio& audio) {
    if (environmentQueue) xQueueReceive(environmentQueue, &environment, 0);
    else environment.status = "Sensor queue failed";
    if (audioQueue) xQueuePeek(audioQueue, &audio, 0);
    else audio.status = "Mic: queue failed";
    if (audio.frames && millis() - audio.measuredAt > 500) audio.status = "Mic: data stale";
}
}

// Temporary pressure-drift diagnostic. Microphone acquisition disabled.
// Environmental sensor worker retained from the project's background.cpp.
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_BME280.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <math.h>
#include "background.h"
namespace KnobBackground {
namespace {
QueueHandle_t environmentQueue = nullptr;
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
}
void suppressClick() {} // No audio worker in this diagnostic firmware.
void begin() {
    // Keep microphone clock static: no PDM clock, I2S/DMA or clap task.
    // The microphone supply remains physically connected.
    pinMode(38, OUTPUT); digitalWrite(38, LOW);
    pinMode(40, OUTPUT); digitalWrite(40, LOW);
    pinMode(39, INPUT);
    environmentQueue = xQueueCreate(1, sizeof(Environment));
    if (environmentQueue && xTaskCreatePinnedToCore(environmentTask, "bme280", 4096, nullptr, 1, nullptr, 0) != pdPASS) {
        Environment e; e.status = "Sensor task failed"; xQueueOverwrite(environmentQueue, &e);
    }
}
void read(Environment& environment, Audio& audio) {
    if (environmentQueue) xQueueReceive(environmentQueue, &environment, 0);
    else environment.status = "Sensor queue failed";
    audio.pairs = 0; audio.lastPairAt = 0;
    audio.status = "Mic off - pressure test";
}
}

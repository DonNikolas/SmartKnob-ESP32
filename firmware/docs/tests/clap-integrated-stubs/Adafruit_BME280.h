#pragma once
#include "Wire.h"
struct Adafruit_BME280 {
    enum {MODE_FORCED,SAMPLING_X1,FILTER_OFF};
    bool begin(int,FakeWire*) { return true; }
    int sensorID() { return 0x60; }
    void setSampling(int,int,int,int,int) {}
    bool takeForcedMeasurement() { return true; }
    float readTemperature() { return 20; }
    float readHumidity() { return 50; }
    float readPressure() { return 100000; }
};

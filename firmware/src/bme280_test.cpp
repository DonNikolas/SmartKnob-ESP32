// Generated with ChatGPT assistance based on my project requirements.
// Basic function test passed on the current SmartKnob PCB.
// Measurement accuracy has not been checked against a reference.

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_BME280.h>
#include <math.h>

constexpr uint8_t SDA_PIN = 48;
constexpr uint8_t SCL_PIN = 41;
constexpr uint8_t BME_ADDRESS = 0x76;

constexpr uint8_t MOTOR_PINS[] = {4, 7, 5, 16, 6, 15};
constexpr uint8_t BACKLIGHT_PIN = 9;

Adafruit_BME280 bme;
bool sensorReady = false;

void scanBus() {
    Serial.println("Scanning I2C bus...");
    unsigned found = 0;

    for (uint8_t address = 1; address < 127; address++) {
        Wire.beginTransmission(address);
        const uint8_t result = Wire.endTransmission();

        if (result == 0) {
            Serial.printf("Device found at 0x%02X\n", address);
            found++;
        }

        delay(1);
    }

    Serial.printf("Devices found: %u\n", found);
}

void startSensor() {
    sensorReady = false;
    Serial.println("\n=== SmartKnob BME280 test ===");
    Serial.println("SDA: GPIO 48 | SCL: GPIO 41 | Expected address: 0x76");

    scanBus();

    if (!bme.begin(BME_ADDRESS, &Wire)) {
        Serial.println("FAIL: BME280 initialization failed at 0x76.");
        Serial.printf("Reported sensor ID: 0x%02lX\n",
                      static_cast<unsigned long>(bme.sensorID()));
        Serial.println("Expected BME280 ID: 0x60.");
        Serial.println("Send r to retry.");
        return;
    }

    Serial.printf("Sensor ID: 0x%02lX\n",
                  static_cast<unsigned long>(bme.sensorID()));

    // Take one measurement on request, then return to sleep.
    bme.setSampling(
        Adafruit_BME280::MODE_FORCED,
        Adafruit_BME280::SAMPLING_X1,
        Adafruit_BME280::SAMPLING_X1,
        Adafruit_BME280::SAMPLING_X1,
        Adafruit_BME280::FILTER_OFF
    );

    sensorReady = true;
    Serial.println("Initialization: PASS");
    Serial.println("Measurements every 2 seconds. Send r to restart.");
}

void readSensor() {
    if (!bme.takeForcedMeasurement()) {
        Serial.println("FAIL: Measurement did not complete.");
        return;
    }

    const float temperature = bme.readTemperature();
    const float humidity = bme.readHumidity();
    const float pressure = bme.readPressure() / 100.0f;

    if (!isfinite(temperature) ||
        !isfinite(humidity) ||
        !isfinite(pressure)) {
        Serial.println("FAIL: Invalid measurement.");
        return;
    }

    Serial.printf(
        "Time: %lu s | Temperature: %.2f C | "
        "Humidity: %.2f %%RH | Pressure: %.2f hPa\n",
        millis() / 1000UL, temperature, humidity, pressure
    );
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

    if (!Wire.begin(SDA_PIN, SCL_PIN, 100000)) {
        Serial.println("FAIL: Could not start I2C.");
        while (true) {
            delay(1000);
        }
    }

    Wire.setTimeOut(50);
    startSensor();
}

void loop() {
    static uint32_t lastMeasurement = 0;

    if (Serial.available() > 0 && Serial.read() == 'r') {
        startSensor();
    }

    if (sensorReady && millis() - lastMeasurement >= 2000) {
        lastMeasurement = millis();
        readSensor();
    }

    delay(10);
}
// Generated with ChatGPT assistance.
// Initial motor driver test. Hardware validation pending.
// Open-loop rotating field: no angle feedback or detent control.

#include <Arduino.h>
#include <SimpleFOC.h>
#include <math.h>

constexpr uint8_t MOTOR_PINS[] = {4, 7, 5, 16, 6, 15};
constexpr uint8_t DIAG_PIN = 17;
constexpr uint8_t BACKLIGHT_PIN = 9;

// Phase order: UH, UL, VH, VL, WH, WL.
BLDCDriver6PWM driver(4, 7, 5, 16, 6, 15);

constexpr float SUPPLY_VOLTAGE = 5.0f;
constexpr float PHASE_AMPLITUDE = 0.30f;
constexpr float ELECTRICAL_HZ = 1.0f;
constexpr float TWO_PI_F = 6.28318530718f;

constexpr uint32_t RUN_TIME_MS = 3000;
constexpr uint32_t RAMP_TIME_MS = 500;

bool driverReady = false;
bool running = false;
int direction = 1;
uint32_t startedAt = 0;

void stopMotor(const char* reason) {
    if (driverReady) {
        driver.disable();
    }

    running = false;
    Serial.print("STOP: ");
    Serial.println(reason);
}

void startMotor(int requestedDirection) {
    if (!driverReady) {
        Serial.println("START BLOCKED: driver initialization failed.");
        return;
    }

    // A running test cannot be extended by repeated start commands.
    if (running) {
        return;
    }

    if (digitalRead(DIAG_PIN) == HIGH) {
        Serial.println("START BLOCKED: TMC6300 DIAG is HIGH.");
        return;
    }

    direction = requestedDirection;
    Serial.println(direction > 0
        ? "START: forward field, maximum 3 seconds."
        : "START: reverse field, maximum 3 seconds.");

    driver.enable();

    // Equal phase voltages produce no commanded voltage difference.
    const float centre = SUPPLY_VOLTAGE * 0.5f;
    driver.setPwm(centre, centre, centre);

    startedAt = millis();
    running = true;
}

void setup() {
    // Hold all motor inputs LOW before configuring PWM.
    for (uint8_t pin : MOTOR_PINS) {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, LOW);
    }

    pinMode(BACKLIGHT_PIN, OUTPUT);
    digitalWrite(BACKLIGHT_PIN, LOW);

    pinMode(DIAG_PIN, INPUT);

    Serial.begin(115200);
    delay(3000);

    Serial.println("\n=== SmartKnob motor test ===");

    driver.voltage_power_supply = SUPPLY_VOLTAGE;
    driver.voltage_limit = SUPPLY_VOLTAGE;
    driver.pwm_frequency = 20000;
    driver.dead_zone = 0.02f;

    driverReady = driver.init() == 1;

    if (!driverReady) {
        Serial.println("FAIL: PWM driver initialization.");
        return;
    }

    driver.disable();

    Serial.println("Driver initialized. Motor outputs disabled.");
    Serial.println("Supply setting: 5 V.");
    Serial.println("Phase amplitude: 0.30 V, not a current limit.");
    Serial.println("Angle sensor is not used by this test.");
    Serial.println("f = forward | b = reverse | x = stop");
    Serial.println("Each run stops automatically after 3 seconds.");
    Serial.printf("TMC6300 DIAG: %s\n",
                  digitalRead(DIAG_PIN) ? "FAULT" : "OK");
}

void loop() {
    // Check stop conditions before processing commands.
    if (running && digitalRead(DIAG_PIN) == HIGH) {
        stopMotor("TMC6300 fault");
    }

    if (running && millis() - startedAt >= RUN_TIME_MS) {
        stopMotor("time limit");
    }

    // Process one character per iteration to keep control responsive.
    if (Serial.available() > 0) {
        const char command = Serial.read();

        if (command == 'x' || command == 'X') {
            stopMotor("user command");
        } else if (command == 'f') {
            startMotor(1);
        } else if (command == 'b') {
            startMotor(-1);
        }
    }

    if (!running) {
        delay(1);
        return;
    }

    const uint32_t elapsed = millis() - startedAt;
    const float seconds = elapsed * 0.001f;

    // Gradually increase and decrease the phase amplitude.
    float ramp = 1.0f;

    if (elapsed < RAMP_TIME_MS) {
        ramp = static_cast<float>(elapsed) / RAMP_TIME_MS;
    }

    const uint32_t remaining =
        elapsed < RUN_TIME_MS ? RUN_TIME_MS - elapsed : 0;

    if (remaining < RAMP_TIME_MS) {
        const float rampDown =
            static_cast<float>(remaining) / RAMP_TIME_MS;
        if (rampDown < ramp) ramp = rampDown;
    }

    const float amplitude = PHASE_AMPLITUDE * ramp;
    const float angle =
        direction * TWO_PI_F * ELECTRICAL_HZ * seconds;
    const float centre = SUPPLY_VOLTAGE * 0.5f;

    driver.setPwm(
        centre + amplitude * sinf(angle),
        centre + amplitude * sinf(angle - TWO_PI_F / 3.0f),
        centre + amplitude * sinf(angle + TWO_PI_F / 3.0f)
    );

    delayMicroseconds(200);
}
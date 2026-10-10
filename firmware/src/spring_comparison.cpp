// SmartKnob-NB: experimental spring A/B comparison, version 5.
// Generated with ChatGPT assistance. Hardware validation pending.
// Architecture reference: Scott Bezek's SmartKnob motor_task.cpp and
// mt6701_sensor.cpp (Copyright 2022 Scott Bezek, Apache-2.0).
// https://github.com/scottbez1/smartknob/tree/master/firmware/src
// SeedLabs motor_foc implementation was also reviewed.
// Changes: custom pins, first-harmonic commutation model from user logs,
// manual start, bounded session, validity gates and independent A/B selection.
// This is NOT an absolute-angle calibration or a correction for weak field.
// Model applies only to the measured PCB/motor/sensor mounting and wiring.
// Full upstream license accompanies this file in the supplied archive.

#include <Arduino.h>
#include <SimpleFOC.h>
#include <driver/spi_master.h>
#include <math.h>

constexpr float TAU_F = 6.28318530718f;
constexpr float PI_F = TAU_F / 2;
constexpr float DEG_TO_RAD_F = PI_F / 180;
constexpr uint8_t MOTOR_PINS[] = {4, 7, 5, 16, 6, 15};
constexpr uint8_t SENSOR_CS = 14, SENSOR_CLK = 21, SENSOR_DATA = 47;
constexpr uint8_t DRIVER_DIAG = 17, BACKLIGHT = 9;
constexpr int PAIRS = 7, SENSOR_SIGN = -1;
constexpr float MAX_VOLTAGE = 0.60f; // Voltage-mode FOC, not a current limit.
constexpr float STIFFNESS = 3.00f; // V/rad of raw sensor displacement.
constexpr float DAMPING = 0.0f; // V per rad/s of raw sensor velocity.
float wrapAngle(float a);
float positiveAngle(float a);

constexpr float DEAD_ZONE = 1 * DEG_TO_RAD_F;
constexpr float DETENT_WIDTH = TAU_F / 4;
constexpr float SNAP_ANGLE = 47 * DEG_TO_RAD_F; // 2-degree switching hysteresis.
constexpr uint32_t HOLD_US = 2000;
bool holdBriefDropouts = false; // Default remains immediate zero command.
bool heldOutput = false;
uint32_t heldTicks = 0, zeroedTicks = 0;
bool zeroTest = false;
float zeroStartAngle = 0;
bool fourDetents = false;
float activeCentre = 0;

// Keep the current detent until the midpoint plus hysteresis is crossed.
float detentError(float angle) {
    float error = wrapAngle(activeCentre - angle);
    if (fourDetents) {
        while (error < -SNAP_ANGLE) {
            activeCentre = positiveAngle(activeCentre + DETENT_WIDTH);
            error = wrapAngle(activeCentre - angle);
        }
        while (error > SNAP_ANGLE) {
            activeCentre = positiveAngle(activeCentre - DETENT_WIDTH);
            error = wrapAngle(activeCentre - angle);
        }
    }
    return error;
}

constexpr float TRAVEL_LIMIT = 45 * DEG_TO_RAD_F;
constexpr uint32_t SESSION_MS = 10000, STALE_US = 5000;

// Least-squares model fitted to profile 01, validated against profile 02.
// Fit: offset_deg = DC + SIN*sin(raw_angle) + COS*cos(raw_angle)
//                  + direction_specific_constant.
// Only the direction-independent terms are used below. The direction term
// is excluded to avoid switching commutation at a direction reversal.
constexpr float OFFSET_DC_DEG = 22.04286664f;
constexpr float OFFSET_SIN_DEG = -18.18597597f;
constexpr float OFFSET_COS_DEG = -24.82943521f;
constexpr float ELECTRICAL_ZERO = (OFFSET_DC_DEG + 270) * DEG_TO_RAD_F;

float wrapAngle(float a) {
    while (a > PI_F) a -= TAU_F;
    while (a < -PI_F) a += TAU_F;
    return a;
}
float positiveAngle(float a) {
    a = fmodf(a, TAU_F);
    return a < 0 ? a + TAU_F : a;
}
float correctionAt(float raw) {
    return DEG_TO_RAD_F * (OFFSET_SIN_DEG * sinf(raw) + OFFSET_COS_DEG * cosf(raw));
}

bool corrected = false; // Baseline mode on every boot.
float angleCache = 0;
class ComparisonSensor : public Sensor {
public:
    using Sensor::init;
    float getSensorAngle() override {
        float correction = corrected ? correctionAt(angleCache) : 0;
        // Then direction * pairs * corrected_angle - zero equals
        // direction * pairs * raw_angle - zero - correction(raw_angle).
        return positiveAngle(angleCache - correction / (SENSOR_SIGN * PAIRS));
    }
};
ComparisonSensor encoder;
BLDCDriver6PWM driver(4, 7, 5, 16, 6, 15);
BLDCMotor motor(PAIRS);
spi_device_handle_t sensorSpi = nullptr;
bool ready = false, running = false, suspended = false, haveAngle = false;
bool good = false, lost = false, centred = false;
float centreRaw = 0, previousRaw = 0, angularSpeed = 0, filterX = 0, filterY = 0;
uint32_t lastGoodUs = 0, previousTickUs = 0, startedMs = 0, resumedUs = 0;
uint32_t suspensionUs = 0, crcErrors = 0, trackLost = 0, recoveries = 0, weakFrames = 0;
float maximumError = 0, maximumVoltage = 0;
uint8_t fieldStatus = 0;
const char* problem = "No sensor sample";

uint8_t crc6(uint32_t data) {
    uint8_t crc = 0;
    for (int bit = 17; bit >= 0; --bit) {
        bool feedback = ((crc >> 5) ^ (data >> bit)) & 1U;
        crc = (crc << 1) & 63;
        if (feedback) crc ^= 3;
    }
    return crc;
}

bool readSensor() {
    good = lost = false;
    spi_transaction_t t = {};
    t.flags = SPI_TRANS_USE_RXDATA; t.length = t.rxlength = 24;
    if (spi_device_polling_transmit(sensorSpi, &t) != ESP_OK) {
        problem = "SPI error"; return false;
    }
    uint32_t frame = (uint32_t(t.rx_data[0]) << 16) | (uint32_t(t.rx_data[1]) << 8) | t.rx_data[2];
    if (frame == 0 || frame == 0xFFFFFFUL) { problem = "Constant sensor frame"; return false; }
    if (crc6(frame >> 6) != (frame & 63)) { ++crcErrors; problem = "CRC error"; return false; }
    fieldStatus = (frame >> 6) & 3;
    if (fieldStatus == 1 || fieldStatus == 3) { problem = "Strong/reserved field"; return false; }
    if (fieldStatus == 2) ++weakFrames; // Visible unresolved warning, as in v6.
    if (frame & (1UL << 9)) { ++trackLost; lost = true; problem = "TRACK LOST"; return false; }
    float raw = (frame >> 10) * (TAU_F / 16384);
    uint32_t now = micros();
    float dt = (now - lastGoodUs) * 1e-6f;
    if (running && haveAngle && fabsf(wrapAngle(raw - previousRaw)) > 5 * DEG_TO_RAD_F) {
        problem = "Unexpected angle jump"; return false;
    }
    float x = cosf(raw), y = sinf(raw);
    if (!haveAngle) { filterX = x; filterY = y; }
    else { filterX += 0.4f * (x - filterX); filterY += 0.4f * (y - filterY); }
    float next = positiveAngle(atan2f(filterY, filterX));
    if (haveAngle && dt > 0 && dt < 0.1f) {
        float speed = wrapAngle(next - angleCache) / dt;
        angularSpeed += dt / (0.020f + dt) * (speed - angularSpeed);
    } else angularSpeed = 0;
    angleCache = next; previousRaw = raw; haveAngle = true;
    lastGoodUs = now; problem = "OK"; good = true;
    return true;
}

void stopTest(const char* reason) {
    if (ready) motor.disable();
    running = suspended = false; heldOutput = false;
    Serial.printf("STOP: %s | Mode: %s\n", reason, corrected ? "CORRECTED" : "BASELINE");
    Serial.printf("Dropouts: %s | Held ticks: %lu | Zeroed ticks: %lu\n",
                  holdBriefDropouts ? "HOLD <2 ms" : "ZERO immediately",
                  (unsigned long)heldTicks, (unsigned long)zeroedTicks);
    Serial.println(zeroTest ? "Test: ZERO PWM" : "Test: SPRING");
    Serial.printf("Maximum displacement: %.2f deg | Maximum command: %.3f V\n",
                  maximumError / DEG_TO_RAD_F, maximumVoltage);
    Serial.printf("CRC: %lu | Lost: %lu | Recoveries: %lu | Weak-field frames: %lu\n",
                  (unsigned long)crcErrors, (unsigned long)trackLost,
                  (unsigned long)recoveries, (unsigned long)weakFrames);
}
void help() {
    Serial.println("n = immediate zero on dropout (default) | h = hold PWM for <2 ms; only while OFF");
    Serial.println("0 = baseline | 1 = correction (selection allowed only while OFF)");
    Serial.println("z = 10-second zero-command PWM test; no centre required. Do not turn rotor.");
    Serial.println("c = set centre while OFF | s = start 10-second test");
    Serial.println("2 = single centre | 4 = four detents at 90 degrees (only while OFF)");
    Serial.println("x = stop | i = stop and help. Centre retained when switching 0/1.");
    Serial.println("Single centre: +/-45 deg limit. Four detents: full rotation allowed. Limit: 0.60 V.");
    Serial.println("Weak field unresolved. Model does not establish absolute angle accuracy.");
}

void handleCommand(char key) {
    if (key == 'x' || key == 'X') { stopTest("User command"); return; }
    if (key == 'i') { if (running) stopTest("Status requested"); help(); return; }
    if (key != '0' && key != '1' && key != 'c' && key != 's' && key != '2' && key != '4' && key != 'z' && key != 'n' && key != 'h') return;
    if (running) { stopTest("Stop before changing mode or centre"); return; }
    if (!ready || !good || micros() - lastGoodUs >= STALE_US || digitalRead(DRIVER_DIAG)) {
        Serial.println("BLOCKED: check sensor and driver status."); return;
    }
    if (key == 'n' || key == 'h') {
        holdBriefDropouts = key == 'h';
        Serial.println(holdBriefDropouts ? "Dropouts: HOLD <2 ms, then zero; OFF at 5 ms."
                                        : "Dropouts: ZERO immediately; OFF at 5 ms.");
        return;
    }
    if (key == '2' || key == '4') {
        fourDetents = key == '4';
        centred = false; // Require a deliberate new reference after mode changes.
        Serial.println(fourDetents ? "Selected: FOUR DETENTS. Set reference with c."
                                  : "Selected: SINGLE CENTRE. Set reference with c.");
        return;
    }
    if (key == '0' || key == '1') {
        corrected = key == '1'; encoder.init();
        Serial.printf("Selected: %s. Motor remains OFF.\n", corrected ? "CORRECTED" : "BASELINE");
        return;
    }
    if (key == 'c') {
        centreRaw = activeCentre = angleCache; centred = true;
        Serial.printf("Centre: %.2f raw sensor deg. Motor OFF.\n", centreRaw / DEG_TO_RAD_F);
        return;
    }
    zeroTest = key == 'z';
    zeroStartAngle = angleCache;
    if (!zeroTest && !centred) { Serial.println("BLOCKED: set centre with c first."); return; }
    activeCentre = centreRaw;
    if (!zeroTest && fabsf(detentError(angleCache)) > 5 * DEG_TO_RAD_F) {
        Serial.println("BLOCKED: return within 5 deg of a selected detent."); return;
    }
    Serial.printf("START: %s | 10 seconds | x stops\n", corrected ? "CORRECTED" : "BASELINE");
    Serial.println(zeroTest ? "Test: ZERO PWM (Uq=0); PWM stays active, motor is not OFF." : "Test: SPRING");
    if (!zeroTest) Serial.println(fourDetents ? "Haptics: FOUR DETENTS" : "Haptics: SINGLE CENTRE");
    Serial.println(holdBriefDropouts ? "Dropout mode: HOLD <2 ms" : "Dropout mode: ZERO immediately");
    heldTicks = zeroedTicks = 0; heldOutput = false;
    crcErrors = trackLost = recoveries = weakFrames = 0;
    maximumError = maximumVoltage = angularSpeed = 0;
    encoder.init();
    motor.enable(); motor.move(0); motor.loopFOC();
    startedMs = millis(); resumedUs = micros(); previousTickUs = micros();
    suspended = false; running = true;
}

void controlTick(bool valid) {
    if (!running) return;
    if (digitalRead(DRIVER_DIAG)) { stopTest("Driver fault"); return; }
    if (millis() - startedMs >= SESSION_MS) { stopTest("10-second session finished"); return; }
    if (!valid && !lost) { stopTest(problem); return; }
    if ((suspended && micros() - suspensionUs >= STALE_US) ||
        (!valid && micros() - lastGoodUs >= STALE_US)) {
        stopTest("Sensor dropout reached 5 ms"); return;
    }
    if (!valid) {
        if (!suspended) { suspended = true; suspensionUs = micros(); }
        // Experimental bounded zero-order hold. Do not use the flagged angle,
        // extrapolate motion, or update FOC while holding the previous PWM.
        // Only hold at low last-known speed; CRC/driver faults still stop above.
        if (holdBriefDropouts && !zeroTest && micros() - lastGoodUs < HOLD_US &&
            fabsf(angularSpeed) < 2.0f) {
            heldOutput = true;
            ++heldTicks;
            return;
        }
        heldOutput = false;
        ++zeroedTicks;
        motor.move(0);
        motor.loopFOC();
        return;
    }
    float error = zeroTest ? wrapAngle(zeroStartAngle - angleCache) : detentError(angleCache);
    maximumError = max(maximumError, fabsf(error));
    if ((zeroTest || !fourDetents) && fabsf(error) > TRAVEL_LIMIT) { stopTest("45-degree travel limit"); return; }
    if (fabsf(angularSpeed) > 15) { stopTest("Speed limit"); return; }
    if (suspended) {
        // A recovery after a hold must not inject a new zero pulse or ramp.
        // If zero was applied, retain the original short recovery ramp.
        if (!heldOutput) { motor.move(0); resumedUs = micros(); }
        suspended = false; heldOutput = false; ++recoveries;
    }
    if (zeroTest) {
        // Keep equal centered phase commands on every tick, including recovery.
        // No spring calculation and no driver enable/disable cycling here.
        motor.move(0);
        motor.loopFOC();
        return;
    }
    float ramp = min(1.0f, (millis() - startedMs) / 500.0f);
    // Short recovery ramp. Recoveries counts resumed valid data, not driver restarts.
    ramp *= min(1.0f, (micros() - resumedUs) / 2000.0f);
    // Quiet zone around the centre; continuous proportional output at its edge.
    float springError = error > DEAD_ZONE ? error - DEAD_ZONE
                      : error < -DEAD_ZONE ? error + DEAD_ZONE : 0;
    float rawTorque = STIFFNESS * springError - DAMPING * angularSpeed;
    // SimpleFOC positive torque follows calibrated shaft direction (CCW here).
    float voltage = constrain(SENSOR_SIGN * rawTorque, -MAX_VOLTAGE, MAX_VOLTAGE) * ramp;
    maximumVoltage = max(maximumVoltage, fabsf(voltage));
    motor.move(voltage);
    motor.loopFOC();
}

void setup() {
    for (uint8_t pin : MOTOR_PINS) { pinMode(pin, OUTPUT); digitalWrite(pin, LOW); }
    pinMode(BACKLIGHT, OUTPUT); digitalWrite(BACKLIGHT, LOW);
    pinMode(DRIVER_DIAG, INPUT);
    Serial.begin(115200); delay(2500);
    Serial.println("\n=== SmartKnob spring comparison v5 - EXPERIMENTAL ===");
    spi_bus_config_t bus = {};
    bus.mosi_io_num = -1; bus.miso_io_num = SENSOR_DATA; bus.sclk_io_num = SENSOR_CLK;
    bus.quadwp_io_num = -1; bus.quadhd_io_num = -1; bus.max_transfer_sz = 4;
    spi_device_interface_config_t device = {};
    device.mode = 1; device.clock_speed_hz = 4000000; device.spics_io_num = SENSOR_CS;
    device.cs_ena_pretrans = 4; device.queue_size = 1;
    if (spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO) != ESP_OK ||
        spi_bus_add_device(SPI2_HOST, &device, &sensorSpi) != ESP_OK) {
        Serial.println("FAIL: SPI initialization"); return;
    }
    for (unsigned i = 0; i < 30; ++i) { readSensor(); delay(1); }
    if (!haveAngle) { Serial.println("FAIL: no usable sensor angle"); return; }
    encoder.init();
    driver.voltage_power_supply = 5; driver.pwm_frequency = 20000; driver.dead_zone = 0.02f;
    if (!driver.init()) { Serial.println("FAIL: driver initialization"); return; }
    motor.linkDriver(&driver); motor.linkSensor(&encoder);
    motor.controller = MotionControlType::torque;
    motor.torque_controller = TorqueControlType::voltage;
    motor.foc_modulation = FOCModulationType::SinePWM;
    motor.sensor_direction = Direction::CCW;
    motor.zero_electric_angle = ELECTRICAL_ZERO;
    motor.voltage_limit = 0; motor.voltage_sensor_align = 0;
    motor.init();
    // Explicit direction/offset skip SimpleFOC's automatic alignment movement.
    bool focReady = motor.initFOC();
    motor.disable();
    if (!focReady) { Serial.println("FAIL: FOC initialization"); return; }
    motor.voltage_limit = MAX_VOLTAGE;
    ready = true; previousTickUs = micros();
    help();
}

void loop() {
    if (!ready) { delay(100); return; }
    // Read at most one command per iteration so x is not followed immediately
    // by a queued restart in the same iteration.
    if (Serial.available()) handleCommand(Serial.read());
    uint32_t now = micros(), dt = now - previousTickUs;
    if (dt < 1000) return;
    previousTickUs = now;
    if (running && dt > 20000) { stopTest("Control loop delayed over 20 ms"); return; }
    if (running && holdBriefDropouts && dt > HOLD_US) {
        stopTest("Hold-mode loop gap exceeded 2 ms"); return;
    }
    bool valid = readSensor();
    controlTick(valid);
    static uint32_t lastReport = 0;
    if (!running && millis() - lastReport >= 2000) {
        lastReport = millis();
        Serial.printf("OFF | Mode: %s | Angle: %.2f deg | Sensor: %s | Field: %s\n",
                      corrected ? "CORRECTED" : "BASELINE", angleCache / DEG_TO_RAD_F,
                      problem, fieldStatus == 2 ? "TOO WEAK" : (fieldStatus == 0 ? "OK" : "INVALID"));
    }
}

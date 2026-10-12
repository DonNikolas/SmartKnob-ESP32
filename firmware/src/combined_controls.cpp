// SmartKnob-NB: combined motor, press, display and LED test, version 1.
// Generated with ChatGPT assistance. Hardware validation pending.
// Architecture reference: Scott Bezek's SmartKnob motor_task.cpp and
// mt6701_sensor.cpp (Copyright 2022 Scott Bezek, Apache-2.0).
// https://github.com/scottbez1/smartknob/tree/master/firmware/src
// SeedLabs motor_foc implementation was also reviewed.
// Changes: custom pins, first-harmonic commutation model from user logs,
// manual start, bounded session, validity gates and independent A/B selection.
// This is NOT an absolute-angle calibration or a correction for weak field.
// Model applies only to the measured PCB/motor/sensor mounting and wiring.
// Upstream license: firmware/docs/licenses/Scott-SmartKnob-Apache-2.0.txt
// v2: 12 detents, optional spring slew limit, separate click voltage, cancel reason.

#include <Arduino.h>
#include <SimpleFOC.h>
#include <driver/spi_master.h>
#include <math.h>
#include "press_input.h"
#include "combined_ui.h"

constexpr float TAU_F = 6.28318530718f;
constexpr float PI_F = TAU_F / 2;
constexpr float DEG_TO_RAD_F = PI_F / 180;
constexpr uint8_t MOTOR_PINS[] = {4, 7, 5, 16, 6, 15};
constexpr uint8_t SENSOR_CS = 14, SENSOR_CLK = 21, SENSOR_DATA = 47;
constexpr uint8_t DRIVER_DIAG = 17, BACKLIGHT = 9;
constexpr int PAIRS = 7, SENSOR_SIGN = -1;
constexpr float SPRING_LIMIT = 1.00f; // Keep the tested spring voltage limit.
constexpr float CLICK_VOLTAGE = 3.00f; // Experimental short pulse, not continuous drive.
constexpr float MOTOR_LIMIT = CLICK_VOLTAGE > SPRING_LIMIT ? CLICK_VOLTAGE : SPRING_LIMIT;
constexpr float SPRING_SLEW = 200.0f; // V/s: full +1 to -1 V transition takes 10 ms.
bool smoothSpring = true; // r = original response, g = slew-limited response.
float springOutput = 0;
uint32_t springOutputUs = 0;
void resetSpringOutput() { springOutput = 0; springOutputUs = micros(); }
float springCommand(float requested) {
    requested = constrain(requested, -SPRING_LIMIT, SPRING_LIMIT);
    uint32_t now = micros();
    // Do not accumulate a large permitted jump across a dropout or click.
    float dt = min(uint32_t(now - springOutputUs), uint32_t(2000)) * 1e-6f;
    springOutputUs = now;
    if (!smoothSpring) springOutput = requested;
    else springOutput += constrain(requested - springOutput, -SPRING_SLEW * dt, SPRING_SLEW * dt);
    return springOutput;
}
constexpr float STIFFNESS = 3.00f; // V/rad of raw sensor displacement.
constexpr float DAMPING = 0.0f; // V per rad/s of raw sensor velocity.
float wrapAngle(float a);
float positiveAngle(float a);

constexpr float DEAD_ZONE = 1 * DEG_TO_RAD_F;
constexpr unsigned DETENT_COUNT = 12;
constexpr float DETENT_WIDTH = TAU_F / DETENT_COUNT;
constexpr float SNAP_ANGLE = DETENT_WIDTH / 2 + 2 * DEG_TO_RAD_F;
constexpr uint32_t HOLD_US = 2000;
bool holdBriefDropouts = true; // Use the tested bounded hold by default.
bool heldOutput = false;
uint32_t heldTicks = 0, zeroedTicks = 0;
bool zeroTest = false;
float zeroStartAngle = 0;
bool multipleDetents = true;
float activeCentre = 0;

// Keep the current detent until the midpoint plus hysteresis is crossed.
float detentError(float angle) {
    float error = wrapAngle(activeCentre - angle);
    if (multipleDetents) {
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
// Scott's normal click uses about 3 ms per direction. Keep this user's tested
// 5 ms duration; only increase click amplitude. No blocking delays.
constexpr uint32_t CLICK_HALF_US = 5000;
static_assert(CLICK_VOLTAGE <= MOTOR_LIMIT, "Click exceeds motor limit");
uint8_t clickStage = 0; // 0=idle, 1=pending, 2=timed pulse
uint32_t clickAt = 0, pressCount = 0, completedClicks = 0, cancelledClicks = 0;
const char* lastCancelReason = "none";
uint32_t lastCancelAgeUs = 0;
void cancelClick(const char* reason) {
    if (clickStage) {
        lastCancelReason = reason;
        lastCancelAgeUs = clickStage == 2 ? micros() - clickAt : 0;
        ++cancelledClicks; clickStage = 0; PressInput::afterClick();
    }
}
bool clickVoltage(float& output) {
    if (!clickStage) return false;
    if (clickStage == 1) { clickStage = 2; clickAt = micros(); }
    const uint32_t age = micros() - clickAt;
    if (age < CLICK_HALF_US) output = CLICK_VOLTAGE;
    else if (age < 2 * CLICK_HALF_US) output = -CLICK_VOLTAGE;
    else {
        output = 0; clickStage = 0; ++completedClicks;
        resumedUs = micros(); PressInput::afterClick();
    }
    return true;
}
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

// Motor/core 1 owns these values. The UI receives copies, never motor objects.
int ledBottom = CombinedUi::LED_SIX_OCLOCK_INDEX;
int ledCcwStep = CombinedUi::LED_CCW_INDEX_STEP;
int rawToCcw = CombinedUi::RAW_TO_CCW;
int confirmedPosition = -1;
uint32_t confirmationCount = 0;
bool alignmentPreview = true;
int previewPosition = 0;

int selectedPosition() {
    if (alignmentPreview) return previewPosition;
    const int steps = lroundf(wrapAngle(activeCentre - centreRaw) / DETENT_WIDTH);
    return CombinedUi::wrapIndex(rawToCcw * steps, CombinedUi::POSITIONS);
}
void publishView() {
    CombinedUi::State view;
    view.position = selectedPosition();
    view.confirmed = confirmedPosition;
    view.confirmations = confirmationCount;
    view.bottomLed = ledBottom; view.ledDirection = ledCcwStep;
    view.running = running;
    view.zeroing = PressInput::collecting;
    view.pressureReady = PressInput::readyToStart();
    view.centred = centred;
    view.alignment = alignmentPreview;
    CombinedUi::publish(view);
}
void stopTest(const char* reason);
// Calibration affects presentation only, never commutation or voltage limits.
bool alignmentCommand(char key) {
    if (key != '[' && key != ']' && key != 'l' && key != 'd' && key != 'p') return false;
    if (running) {
        stopTest("Stop before alignment; repeat command while OFF");
        return true;
    }
    if (key == '[' || key == ']') {
        ledBottom = CombinedUi::wrapIndex(ledBottom + (key == ']' ? 1 : -1), 36);
        alignmentPreview = true; previewPosition = 0;
    } else if (key == 'l') {
        ledCcwStep = -ledCcwStep;
        alignmentPreview = true; previewPosition = 0;
    } else if (key == 'd') {
        rawToCcw = -rawToCcw; centred = false;
        alignmentPreview = true; previewPosition = 0;
    } else {
        alignmentPreview = true;
        previewPosition = CombinedUi::wrapIndex(previewPosition + 1, 12);
    }
    Serial.printf("Alignment (RAM only): bottom LED=%d | LED CCW step=%d | raw-to-CCW=%d | position=%d\n",
                  ledBottom, ledCcwStep, rawToCcw, selectedPosition() + 1);
    publishView();
    return true;
}

void stopTest(const char* reason) {
    if (ready) motor.disable();
    cancelClick(reason); PressInput::resetDetection(); resetSpringOutput();
    running = suspended = false; heldOutput = false;
    publishView();
    Serial.printf("STOP: %s | Mode: %s\n", reason, corrected ? "CORRECTED" : "BASELINE");
    Serial.printf("Presses: %lu | Clicks completed: %lu | Clicks cancelled: %lu\n",
                  (unsigned long)pressCount, (unsigned long)completedClicks,
                  (unsigned long)cancelledClicks);
    Serial.printf("Confirmed position: %d | Total confirmations: %lu\n", confirmedPosition + 1, (unsigned long)confirmationCount);
    Serial.printf("Last click cancellation: %s | Pulse age: %lu us\n",
                  lastCancelReason, (unsigned long)lastCancelAgeUs);
    Serial.printf("Spring response: %s | Spring limit: %.2f V | Click: %.2f V, %lu us per direction\n",
                  smoothSpring ? "SMOOTH" : "DIRECT", SPRING_LIMIT, CLICK_VOLTAGE,
                  (unsigned long)CLICK_HALF_US);
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
    Serial.println("Combined test: 12 positions, display + LED marker, press to confirm.");
    Serial.println("LED alignment while OFF: [ / ] move position 1 to 6 o'clock.");
    Serial.println("p previews the next position; l reverses LED order; d reverses UI encoder direction.");
    Serial.println("c exits preview and sets position 1. Alignment is RAM-only; copy printed settings to combined_ui.h.");
    Serial.println("t = unloaded HX711 zero (motor OFF); +/- = threshold +/-1000 while OFF");
    Serial.printf("Press threshold: %ld; one confirmation per press, release required.\n", PressInput::threshold);
    Serial.printf("Click: +/-%.2f V, %lu us per direction, then return to detents.\n",
                  CLICK_VOLTAGE, (unsigned long)CLICK_HALF_US);
    Serial.println("r = direct spring | g = smooth spring (default); select only while OFF");
    Serial.println("n = immediate zero on dropout | h = hold PWM for <2 ms (default); only while OFF");
    Serial.println("0 = baseline | 1 = correction (selection allowed only while OFF)");
    Serial.println("z = 10-second zero-command PWM test; no centre required. Do not turn rotor.");
    Serial.println("c = set centre while OFF | s = start 10-second test");
    Serial.println("2 = single centre | 4 = twelve detents at 30 degrees (only while OFF)");
    Serial.println("x = stop | i = stop and help. Centre retained when switching 0/1.");
    Serial.println("Single centre: +/-45 deg limit. Twelve detents: full rotation allowed. Spring limit: 1.00 V.");
    Serial.println("Weak field unresolved. Model does not establish absolute angle accuracy.");
}

void handleCommand(char key) {
    if (alignmentCommand(key)) return;
    if (key == 'x' || key == 'X') { stopTest("User command"); return; }
    if (key == 't' || key == '+' || key == '-') {
        if (running) { stopTest("Stop before pressure settings; repeat command while OFF"); return; }
        if (key == 't') PressInput::tare(); else PressInput::changeThreshold(key);
        return;
    }
    if (key == 'i') { if (running) stopTest("Status requested"); help(); return; }
    if (key != '0' && key != '1' && key != 'c' && key != 's' && key != '2' && key != '4' && key != 'z' && key != 'n' && key != 'h' && key != 'r' && key != 'g') return;
    if (running) { stopTest("Stop before changing mode or centre"); return; }
    if (!ready || !good || micros() - lastGoodUs >= STALE_US || digitalRead(DRIVER_DIAG)) {
        Serial.println("BLOCKED: check sensor and driver status."); return;
    }
    if (key == 'r' || key == 'g') {
        smoothSpring = key == 'g'; resetSpringOutput();
        Serial.println(smoothSpring ? "Selected: SMOOTH spring." : "Selected: DIRECT spring.");
        return;
    }
    if (key == 'n' || key == 'h') {
        holdBriefDropouts = key == 'h';
        Serial.println(holdBriefDropouts ? "Dropouts: HOLD <2 ms, then zero; OFF at 5 ms."
                                        : "Dropouts: ZERO immediately; OFF at 5 ms.");
        return;
    }
    if (key == '2' || key == '4') {
        multipleDetents = key == '4';
        centred = false; // Require a deliberate new reference after mode changes.
        Serial.println(multipleDetents ? "Selected: TWELVE DETENTS. Set reference with c."
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
        alignmentPreview = false; confirmedPosition = -1;
        Serial.printf("Centre: %.2f raw sensor deg. Motor OFF.\n", centreRaw / DEG_TO_RAD_F);
        return;
    }
    if (key == 's' && alignmentPreview) { Serial.println("BLOCKED: exit LED preview with c."); return; }
    if (key == 's' && !PressInput::readyToStart()) {
        Serial.println("BLOCKED: tare with t, then wait for fresh full HX711 filter."); return;
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
    if (!zeroTest) Serial.println(multipleDetents ? "Haptics: TWELVE DETENTS" : "Haptics: SINGLE CENTRE");
    Serial.println(holdBriefDropouts ? "Dropout mode: HOLD <2 ms" : "Dropout mode: ZERO immediately");
    Serial.println(smoothSpring ? "Spring response: SMOOTH" : "Spring response: DIRECT");
    lastCancelReason = "none"; lastCancelAgeUs = 0;
    resetSpringOutput();
    pressCount = completedClicks = cancelledClicks = 0; clickStage = 0;
    PressInput::resetDetection();
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
    if (!zeroTest && (!PressInput::valid || millis() - PressInput::lastData >= PressInput::TIMEOUT_MS)) {
        stopTest("HX711 unavailable; tare again if needed"); return;
    }
    if (digitalRead(DRIVER_DIAG)) { stopTest("Driver fault"); return; }
    if (millis() - startedMs >= SESSION_MS) { stopTest("10-second session finished"); return; }
    if (!valid && !lost) { stopTest(problem); return; }
    if ((suspended && micros() - suspensionUs >= STALE_US) ||
        (!valid && micros() - lastGoodUs >= STALE_US)) {
        stopTest("Sensor dropout reached 5 ms"); return;
    }
    if (!valid) {
        // Never carry a click pulse through an invalid sensor frame.
        const bool interruptedClick = clickStage != 0;
        if (interruptedClick) cancelClick("Sensor TRACK LOST");
        if (!suspended) { suspended = true; suspensionUs = micros(); }
        // Experimental bounded zero-order hold. Do not use the flagged angle,
        // extrapolate motion, or update FOC while holding the previous PWM.
        // Only hold at low last-known speed; CRC/driver faults still stop above.
        if (!interruptedClick && holdBriefDropouts && !zeroTest && micros() - lastGoodUs < HOLD_US &&
            fabsf(angularSpeed) < 2.0f) {
            heldOutput = true;
            ++heldTicks;
            return;
        }
        heldOutput = false;
        ++zeroedTicks;
        resetSpringOutput();
        motor.move(0);
        motor.loopFOC();
        return;
    }
    float error = zeroTest ? wrapAngle(zeroStartAngle - angleCache) : detentError(angleCache);
    maximumError = max(maximumError, fabsf(error));
    if ((zeroTest || !multipleDetents) && fabsf(error) > TRAVEL_LIMIT) { stopTest("45-degree travel limit"); return; }
    if (fabsf(angularSpeed) > 15) { stopTest("Speed limit"); return; }
    if (suspended) {
        // A recovery after a hold must not inject a new zero pulse or ramp.
        // If zero was applied, retain the original short recovery ramp.
        if (!heldOutput) { motor.move(0); resetSpringOutput(); resumedUs = micros(); }
        suspended = false; heldOutput = false; ++recoveries;
    }
    if (zeroTest) {
        resetSpringOutput();
        // Keep equal centered phase commands on every tick, including recovery.
        // No spring calculation and no driver enable/disable cycling here.
        motor.move(0);
        motor.loopFOC();
        return;
    }
    float pulse = 0;
    if (clickVoltage(pulse)) {
        resetSpringOutput(); // The crisp click bypasses spring smoothing.
        maximumVoltage = max(maximumVoltage, fabsf(pulse));
        motor.move(pulse); motor.loopFOC();
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
    float voltage = constrain(SENSOR_SIGN * rawTorque, -SPRING_LIMIT, SPRING_LIMIT) * ramp;
    voltage = springCommand(voltage);
    maximumVoltage = max(maximumVoltage, fabsf(voltage));
    motor.move(voltage);
    motor.loopFOC();
}

void setup() {
    for (uint8_t pin : MOTOR_PINS) { pinMode(pin, OUTPUT); digitalWrite(pin, LOW); }
    pinMode(BACKLIGHT, OUTPUT); digitalWrite(BACKLIGHT, LOW);
    pinMode(DRIVER_DIAG, INPUT);
    Serial.begin(115200); delay(2500);
    Serial.println("\n=== SmartKnob combined controls v1 - EXPERIMENTAL ===");
    if (!CombinedUi::begin()) {
        Serial.println("FAIL: display/UI initialization. Motor remains disabled."); return;
    }
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
    motor.voltage_limit = MOTOR_LIMIT;
    PressInput::begin();
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
    if (running && (holdBriefDropouts || clickStage) && dt > HOLD_US) {
        stopTest("Hold-mode loop gap exceeded 2 ms"); return;
    }
    bool valid = readSensor();
    // Follow selection while OFF too, so orientation can be checked without force.
    if (!running && valid && centred && !alignmentPreview) detentError(angleCache);
    controlTick(valid);
    // Skip ADC reads during the timed click pulse. Read only when ready,
    // and never print telemetry while active: serial blocking affects FOC.
    if (!clickStage) {
        const bool allowPress = running && !zeroTest && !suspended && valid &&
                                millis() - startedMs >= 500;
        if (PressInput::poll(allowPress)) {
            ++pressCount; clickStage = 1;
            confirmedPosition = selectedPosition(); ++confirmationCount;
        }
        if (running && !zeroTest && !PressInput::valid) stopTest("HX711 fault");
    }
    static uint32_t lastView = 0;
    if (millis() - lastView >= 20) {
        lastView = millis(); publishView();
    }
    static uint32_t lastReport = 0;
    if (!running && millis() - lastReport >= 2000) {
        lastReport = millis();
        Serial.printf("HX711 | Zero: %s | Filter: %u/8 | Magnitude: %ld | Threshold: %ld | Status: %s\n",
                      PressInput::valid ? "SET" : "NOT SET", PressInput::count,
                      PressInput::magnitude, PressInput::threshold,
                      PressInput::fault ? PressInput::fault : (PressInput::collecting ? "ZEROING" : "OK"));
        Serial.printf("OFF | Mode: %s | Angle: %.2f deg | Sensor: %s | Field: %s\n",
                      corrected ? "CORRECTED" : "BASELINE", angleCache / DEG_TO_RAD_F,
                      problem, fieldStatus == 2 ? "TOO WEAK" : (fieldStatus == 0 ? "OK" : "INVALID"));
    }
}

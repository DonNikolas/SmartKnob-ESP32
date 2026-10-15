// SmartKnob app GUI v1. Generated with ChatGPT assistance.
// Motor commutation adapted from the project's tested combined_controls.cpp.
// Architecture reference: Scott Bezek SmartKnob, Copyright 2022 Scott Bezek,
// Apache-2.0. See docs/licenses/Scott-SmartKnob-Apache-2.0.txt.
// The measured commutation model is specific to this PCB/motor mounting.
// Weak magnetic field remains unresolved. Voltage control is not current control.
#include <Arduino.h>
#include <SimpleFOC.h>
#include <driver/spi_master.h>
#include <math.h>
#include <string.h>
#include "app_gui/model.h"
#include "app_gui/pressure.h"
#include "app_gui/view.h"
#include "app_gui/settings_store.h"

constexpr float TAU_F = 6.28318530718f, PI_F = TAU_F / 2, DEG_TO_RAD_F = PI_F / 180;
constexpr uint8_t MOTOR_PINS[] = {4, 7, 5, 16, 6, 15};
constexpr uint8_t SENSOR_CS = 14, SENSOR_CLK = 21, SENSOR_DATA = 47, DRIVER_DIAG = 17;
constexpr int PAIRS = 7, SENSOR_SIGN = -1;
constexpr float SPRING_LIMIT = 1.00f, CLICK_VOLTAGE = 3.00f;
constexpr float STIFFNESS = 3.00f, SPRING_SLEW = 200.0f;
constexpr float DEAD_ZONE = DEG_TO_RAD_F;
constexpr uint32_t HOLD_US = 2000, STALE_US = 5000, CLICK_HALF_US = 5000;
constexpr float ELECTRICAL_ZERO = (22.04286664f + 270) * DEG_TO_RAD_F;
// Same baseline commutation as the tested combined controls, not a new calibration.
KnobApp::Model app;
KnobPressure::Input pressure;
KnobSettings::Record savedSettings, observedSettings;
uint32_t settingsChangedAt = 0, settingsAttemptAt = 0, settingsQuietAt = 0;
bool settingsAttempted = false;
bool ready = false, running = false, suspended = false, heldOutput = false;
bool good = false, lost = false, haveAngle = false, latchedStop = false;
float angleCache = 0, previousRaw = 0, angularSpeed = 0, filterX = 0, filterY = 0;
float activeCentre = 0, springOutput = 0, standbyAngle = 0;
uint32_t lastGoodUs = 0, previousTickUs = 0, startedMs = 0, resumedUs = 0;
uint32_t suspensionUs = 0, springOutputUs = 0, startupAt = 0;
uint32_t crcErrors = 0, trackLost = 0, weakFrames = 0, recoveries = 0;
uint32_t heldTicks = 0, zeroedTicks = 0, completedClicks = 0, cancelledClicks = 0;
uint8_t fieldStatus = 0, clickStage = 0;
uint32_t clickAt = 0;
bool tareStarted = false, consumeGesture = false;
const char* problem = "No sensor sample";
float wrapAngle(float a) { while (a > PI_F) a -= TAU_F; while (a < -PI_F) a += TAU_F; return a; }
float positiveAngle(float a) { a = fmodf(a, TAU_F); return a < 0 ? a + TAU_F : a; }
class GuiSensor : public Sensor {
public:
    using Sensor::init;
    float getSensorAngle() override { return angleCache; }
};
GuiSensor encoder;
BLDCDriver6PWM driver(4, 7, 5, 16, 6, 15);
BLDCMotor motor(PAIRS);
spi_device_handle_t sensorSpi = nullptr;
void resetSpring() { springOutput = 0; springOutputUs = micros(); }
float springCommand(float value) {
    value = constrain(value, -SPRING_LIMIT, SPRING_LIMIT);
    uint32_t now = micros();
    float dt = min(uint32_t(now - springOutputUs), uint32_t(2000)) * 1e-6f;
    springOutputUs = now;
    springOutput += constrain(value - springOutput, -SPRING_SLEW * dt, SPRING_SLEW * dt);
    return springOutput;
}
void publish() {
    app.state.motorOn = running;
    app.state.zeroing = pressure.zeroing;
    app.state.zeroSamples = pressure.zeroCount;
    app.state.weakField = fieldStatus == 2;
    // Preserve continuity when the selected detent advances and its centre moves.
    // A positive offset follows the same CCW direction as app selection and LEDs.
    app.state.homeOffset = app.state.screen == KnobApp::Screen::Home && running
        ? constrain(KnobApp::RAW_TO_CCW * wrapAngle(angleCache - activeCentre) /
                    (TAU_F / app.slotsPerTurn()), -0.75f, 0.75f) : 0;
    KnobView::publish(app.state);
}
void cancelClick() {
    if (clickStage) { clickStage = 0; ++cancelledClicks; pressure.afterClick(); }
}
void disableMotor() {
    if (ready) motor.disable();
    cancelClick(); running = suspended = heldOutput = false; resetSpring();
}
void stop(const char* reason, bool fault = true) {
    disableMotor(); latchedStop = true; pressure.resetGesture();
    app.state.screen = fault ? KnobApp::Screen::Fault : KnobApp::Screen::Stopped;
    strncpy(app.state.reason, reason, sizeof(app.state.reason) - 1);
    app.state.reason[sizeof(app.state.reason) - 1] = 0;
    ++app.state.revision; publish();
    // Serial output only after power output has been disabled.
    Serial.printf("STOP: %s\nCRC: %lu | Lost: %lu | Recovered: %lu | Clicks: %lu | Cancelled: %lu\n",
        reason, (unsigned long)crcErrors, (unsigned long)trackLost,
        (unsigned long)recoveries, (unsigned long)completedClicks, (unsigned long)cancelledClicks);
}
void anchor() {
    activeCentre = angleCache; resetSpring(); resumedUs = micros();
}
void startMotor() {
    if (!ready || latchedStop || !good || micros() - lastGoodUs >= STALE_US ||
        digitalRead(DRIVER_DIAG) || !pressure.releasedReady(millis())) return;
    encoder.init(); anchor(); angularSpeed = 0;
    motor.enable(); motor.move(0); motor.loopFOC();
    previousTickUs = resumedUs = micros(); startedMs = millis();
    suspended = heldOutput = false; running = true;
}
void changedView(KnobApp::Screen before) {
    if (app.state.screen == before) return;
    anchor();
    if (app.state.screen == KnobApp::Screen::Standby) {
        standbyAngle = angleCache; disableMotor();
    }
}
// Returns a spring error. Logical app changes and physical detents share one owner.
float detentError() {
    float width = TAU_F / app.slotsPerTurn();
    float snap = width / 2 + 2 * DEG_TO_RAD_F;
    float error = wrapAngle(activeCentre - angleCache);
    if (pressure.pressed || consumeGesture) return error;
    while (error < -snap || error > snap) {
        const int rawStep = error < 0 ? 1 : -1;
        const int direction = rawStep * KnobApp::RAW_TO_CCW;
        if (!app.canTurn(direction)) break; // Bounded value: elastic end stop.
        const auto before = app.state.screen;
        activeCentre = positiveAngle(activeCentre + rawStep * width);
        app.turn(direction, millis());
        changedView(before);
        if (before != app.state.screen) return 0;
        error = wrapAngle(activeCentre - angleCache);
    }
    return error;
}
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


bool clickVoltage(float& value) {
    if (!clickStage) return false;
    if (clickStage == 1) { clickStage = 2; clickAt = micros(); }
    const uint32_t age = micros() - clickAt;
    if (age < CLICK_HALF_US) value = CLICK_VOLTAGE;
    else if (age < 2 * CLICK_HALF_US) value = -CLICK_VOLTAGE;
    else { value = 0; clickStage = 0; ++completedClicks; pressure.afterClick(); resumedUs = micros(); }
    return true;
}
void control(bool valid) {
    if (!running) return;
    if (!pressure.valid || millis() - pressure.lastData >= pressure.TIMEOUT_MS) { stop("Pressure sensor unavailable"); return; }
    if (digitalRead(DRIVER_DIAG)) { stop("Motor driver fault"); return; }
    if (!valid && !lost) { stop(problem); return; }
    if ((suspended && micros() - suspensionUs >= STALE_US) || (!valid && micros() - lastGoodUs >= STALE_US)) {
        stop("Sensor dropout reached 5 ms"); return;
    }
    if (!valid) {
        const bool interrupted = clickStage != 0;
        if (interrupted) cancelClick();
        if (!suspended) { suspended = true; suspensionUs = micros(); }
        if (!interrupted && micros() - lastGoodUs < HOLD_US && fabsf(angularSpeed) < 2) {
            heldOutput = true; ++heldTicks; return;
        }
        heldOutput = false; ++zeroedTicks; resetSpring(); motor.move(0); motor.loopFOC(); return;
    }
    if (fabsf(angularSpeed) > 15) { stop("Speed limit"); return; }
    if (suspended) {
        if (!heldOutput) { motor.move(0); resetSpring(); resumedUs = micros(); }
        suspended = heldOutput = false; ++recoveries;
    }
    float pulse = 0;
    if (clickVoltage(pulse)) { resetSpring(); motor.move(pulse); motor.loopFOC(); return; }
    float error = detentError();
    if (app.state.screen == KnobApp::Screen::Value && fabsf(error) > 45 * DEG_TO_RAD_F) {
        stop("Value end-stop travel exceeded"); return;
    }
    float springError = error > DEAD_ZONE ? error - DEAD_ZONE : error < -DEAD_ZONE ? error + DEAD_ZONE : 0;
    float ramp = min(1.0f, (millis() - startedMs) / 500.0f) * min(1.0f, (micros() - resumedUs) / 2000.0f);
    motor.move(springCommand(SENSOR_SIGN * STIFFNESS * springError * ramp)); motor.loopFOC();
}
void handleGesture(KnobPressure::Event event) {
    using E = KnobPressure::Event;
    if (event == E::None || latchedStop) return;
    const auto before = app.state.screen;
    if (event == E::Down) {
        app.touch(millis());
        if (before == KnobApp::Screen::Standby) {
            consumeGesture = true; app.activate(millis()); anchor();
        } else if (running) {
            ++app.state.clicks; KnobBackground::suppressClick(); clickStage = 1;
        }
    } else if (consumeGesture) {
        if (event == E::Short) consumeGesture = false;
    } else if (event == E::Short) app.shortPress(millis());
    else if (event == E::Long) app.longPress(millis());
    changedView(before);
}
void serviceSettings() {
    auto current = KnobSettings::capture(app.state);
    const uint32_t now = millis();
    if (!KnobSettings::equal(current, observedSettings)) {
        observedSettings = current; settingsChangedAt = now;
    }
    app.state.settingsPending = !KnobSettings::equal(current, savedSettings);
    if (!app.state.settingsPending) { app.state.settingsFailed = false; return; }
    // Coalesce changes, including values in an open editor. Require a quiet,
    // released interval before disabling the motor and writing the settings.
    if (pressure.pressed || clickStage || fabsf(angularSpeed) > 0.15f) settingsQuietAt = now;
    if (now - settingsChangedAt < 2000 || now - app.state.activityAt < 2000 ||
        pressure.pressed || clickStage || !good || !pressure.releasedReady(now) ||
        fabsf(angularSpeed) > 0.15f || now - settingsQuietAt < 500) return;
    if (app.state.settingsFailed && settingsAttempted && now-settingsAttemptAt < 30000) return;
    // NVS writes may stall both cores. Disable PWM BEFORE invoking Preferences.
    disableMotor(); KnobBackground::suppressClick(); publish();
    const bool ok = KnobSettings::save(current);
    settingsAttemptAt = millis(); settingsAttempted = true;
    if (ok) savedSettings = current;
    app.state.settingsPending = !ok; app.state.settingsFailed = !ok;
    if (!ok) Serial.println("Settings save failed; retry when idle. Motor is OFF during writes.");
    // Next iteration obtains a fresh encoder sample before the normal guarded start.
    angularSpeed = 0; previousTickUs = micros(); publish();
}
void setup() {
    for (uint8_t pin : MOTOR_PINS) { pinMode(pin, OUTPUT); digitalWrite(pin, LOW); }
    pinMode(9, OUTPUT); digitalWrite(9, LOW); pinMode(DRIVER_DIAG, INPUT);
    Serial.begin(115200);
    KnobSettings::load(app.state);
    savedSettings = observedSettings = KnobSettings::capture(app.state);
    if (!KnobView::begin()) { stop("GUI initialization failed"); return; }
    spi_bus_config_t bus = {};
    bus.mosi_io_num = -1; bus.miso_io_num = SENSOR_DATA; bus.sclk_io_num = SENSOR_CLK;
    bus.quadwp_io_num = -1; bus.quadhd_io_num = -1; bus.max_transfer_sz = 4;
    spi_device_interface_config_t device = {};
    device.mode = 1; device.clock_speed_hz = 4000000; device.spics_io_num = SENSOR_CS;
    device.cs_ena_pretrans = 4; device.queue_size = 1;
    if (spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO) != ESP_OK ||
        spi_bus_add_device(SPI2_HOST, &device, &sensorSpi) != ESP_OK) { stop("Encoder SPI initialization failed"); return; }
    for (unsigned i = 0; i < 30; ++i) { readSensor(); delay(1); }
    if (!haveAngle) { stop("No usable encoder angle"); return; }
    encoder.init();
    driver.voltage_power_supply = 5; driver.pwm_frequency = 20000; driver.dead_zone = 0.02f;
    if (!driver.init()) { stop("Motor driver initialization failed"); return; }
    motor.linkDriver(&driver); motor.linkSensor(&encoder);
    motor.controller = MotionControlType::torque; motor.torque_controller = TorqueControlType::voltage;
    motor.foc_modulation = FOCModulationType::SinePWM; motor.sensor_direction = Direction::CCW;
    motor.zero_electric_angle = ELECTRICAL_ZERO;
    motor.voltage_limit = 0; motor.voltage_sensor_align = 0; motor.init();
    bool ok = motor.initFOC(); motor.disable();
    if (!ok) { stop("FOC initialization failed"); return; }
    motor.voltage_limit = CLICK_VOLTAGE;
    pressure.begin(); ready = true; startupAt = millis(); previousTickUs = micros();
    Serial.println("SmartKnob app GUI: release knob for automatic zero. x = stop, r = restart while stopped.");
    publish();
}
void loop() {
    if (Serial.available()) {
        char key = Serial.read();
        if (key == 'x' || key == 'X') stop("Stopped by user", false);
        else if (key == 'r' && latchedStop) { disableMotor(); ESP.restart(); }
    }
    app.updateTimer(millis());
    if (!ready || latchedStop) {
        static uint32_t lastStoppedPublish = 0;
        if (millis()-lastStoppedPublish >= 250) { lastStoppedPublish=millis(); publish(); }
        delay(5); return;
    }
    uint32_t now = micros(), gap = now - previousTickUs;
    if (gap < 1000) return;
    previousTickUs = now;
    if (running && gap > HOLD_US) { stop("Control loop gap exceeded 2 ms"); return; }
    bool valid = readSensor();
    // Also report faults during startup/standby rather than silently waiting.
    if (digitalRead(DRIVER_DIAG)) { stop("Motor driver fault"); return; }
    if (!valid && !lost) { stop(problem); return; }
    if (!valid && micros() - lastGoodUs >= STALE_US) { stop("Sensor dropout reached 5 ms"); return; }
    control(valid);
    if (latchedStop) return;
    if (!clickStage) {
        // A brief encoder dropout must not discard a completed pressure gesture.
        // The motor path independently cancels pulses on invalid encoder data.
        bool gestures = app.state.screen != KnobApp::Screen::Boot &&
                        (!running || millis() - startedMs >= 500);
        // During startup, arm on a stable released input, but consume every event.
        const auto event = pressure.poll(true);
        if (gestures) handleGesture(event);
        else if (event == KnobPressure::Event::Down) consumeGesture = true;
        if (pressure.fault) { stop(pressure.fault); return; }
    }
    if (consumeGesture && !pressure.pressed) consumeGesture = false;
    if (app.state.screen == KnobApp::Screen::Boot) {
        if (!tareStarted && millis() - startupAt >= 3000) { tareStarted = true; pressure.tare(); }
        if (tareStarted && pressure.releasedReady(millis()) && valid && fabsf(angularSpeed) < 0.2f) {
            app.activate(millis()); anchor(); startMotor();
        }
    } else {
        const auto before = app.state.screen;
        app.tick(millis()); changedView(before);
        if (app.state.screen == KnobApp::Screen::Standby && valid &&
            fabsf(wrapAngle(angleCache - standbyAngle)) > 8 * DEG_TO_RAD_F) {
            app.activate(millis()); anchor(); // Consume wake movement.
        }
        if (app.interactive() && !running && !consumeGesture) startMotor();
    }
    if (app.state.screen != KnobApp::Screen::Boot) serviceSettings();
    static uint32_t lastPublish = 0;
    if (millis() - lastPublish >= 10) { lastPublish = millis(); publish(); }
}

// SmartKnob-NB v6: calibration only; spring output deliberately unavailable.
// Calibration approach adapted from Scott Bezek's SmartKnob (Apache-2.0).
// Original: Copyright 2022 Scott Bezek. Full upstream license is below.
// https://github.com/scottbez1/smartknob/blob/master/firmware/src/motor_task.cpp
// Compared with SeedLabs-it/smartknob-firmware, firmware/src/motor_foc/.
// Changes generated with ChatGPT assistance, 2026-10-09:
// nonblocking state machine, custom PCB pins, bounded voltage, two-direction
// measurement, CRC/status checks, cancellation and calibration quality checks.
// Requires Simple FOC 2.3.3 and Arduino-ESP32 2.0.17.
// No current sensing: voltage limits do not constitute measured current limits.
// Hardware validation pending. Results are RAM-only and never start the spring.
// v6: full mechanical turn in each direction, 800 ms holds, early/late
// angle measurements. This is a diagnostic profile, NOT a correction table.
// The motor field is not an independent precision angle reference.

#include <Arduino.h>
#include <SimpleFOC.h>
#include <driver/spi_master.h>
#include <math.h>

constexpr float TAU_F = 6.28318530718f;
constexpr float PI_F = TAU_F / 2;
constexpr float RAD_TO_DEGREES = 180 / PI_F;
constexpr uint8_t MOTOR_PINS[] = {4, 7, 5, 16, 6, 15};
constexpr uint8_t SENSOR_CS = 14, SENSOR_CLK = 21, SENSOR_DATA = 47;
constexpr uint8_t DRIVER_DIAG = 17, BACKLIGHT = 9;
constexpr unsigned MEASUREMENT_TURNS = 20;
constexpr uint32_t SENSOR_TIMEOUT_US = 5000;
constexpr uint32_t CALIBRATION_TIMEOUT_MS = 180000;
constexpr float ELECTRICAL_SPEED = TAU_F; // One electrical turn per second.
constexpr float TEST_VOLTAGE = 0.50f; // Retain the last tested amplitude.

BLDCDriver6PWM driver(4, 7, 5, 16, 6, 15);
BLDCMotor motor(1); // Temporary: commanded open-loop radians are electrical.
spi_device_handle_t sensorSpi = nullptr;

enum class Stage { Off, Align, DirectionMove, DirectionHold, PreMove, PreHold,
                   Forward, ForwardHold, Reverse, ReverseHold, OffsetMove,
                   OffsetHold };
Stage stage = Stage::Off;
bool ready = false, haveAngle = false, sampleGood = false;
bool sensorLost = false, gapOpen = false;
uint8_t fieldStatus = 0;
float rawAngle = 0, filteredAngle = 0, continuousAngle = 0, previousFiltered = 0;
float filterX = 0, filterY = 0;
uint32_t lastGoodUs = 0, gapStartUs = 0, longestGapUs = 0;
uint32_t crcErrors = 0, lostFrames = 0, weakFrames = 0, samplesRead = 0;
uint32_t stageStartedMs = 0, testStartedMs = 0, previousTickUs = 0;
const char* sampleError = "No sensor sample";

float commandAngle = 0, destination = 0;
float directionStart = 0, mechanicalStart = 0, mechanicalEnd = 0;
float forwardEstimate = 0, reverseEstimate = 0;
int measuredDirection = 0, measuredPairs = 0;
float averageSum = 0;
unsigned averageCount = 0;
unsigned offsetStep = 0, offsetSteps = 0, offsetPass = 0;
float offsetBase = 0, offsetX[2] = {}, offsetY[2] = {};
unsigned offsetCount[2] = {};
constexpr unsigned PROFILE_INTERVALS = 32;
constexpr unsigned MAX_PROFILE_ROWS = 2 * (PROFILE_INTERVALS + 1);
struct ProfileRow {
    unsigned pass, step;
    float commandedElectrical, measuredMechanical, offsetElectrical;
    float settlingMechanical, lateSpanMechanical;
    unsigned earlySamples, lateSamples;
};
ProfileRow profile[MAX_PROFILE_ROWS];
unsigned profileCount = 0;
float earlySum = 0, lateMin = 0, lateMax = 0;
unsigned earlyCount = 0;
float maxSettling = 0, maxLateSpan = 0;

void printProfile() {
    Serial.println("=== OFFSET PROFILE v6 - MOTOR OFF ===");
    Serial.println("Early window: 100-200 ms; late window: 700-800 ms after arrival.");
    Serial.println("Angles in degrees. Offset is electrical; settling and span are mechanical.");
    Serial.println("This profile includes motor, mechanics and sensor errors; it is not ground truth.");
    Serial.println("pass,step,command_elec_deg,sensor_mech_deg,offset_elec_deg,late_minus_early_mech_deg,late_span_mech_deg,early_samples,late_samples");
    for (unsigned i = 0; i < profileCount; ++i) {
        const ProfileRow &r = profile[i];
        Serial.printf("%s,%u,%.3f,%.3f,%.3f,%.4f,%.4f,%u,%u\n",
                      r.pass == 0 ? "FWD" : "REV", r.step,
                      r.commandedElectrical, r.measuredMechanical, r.offsetElectrical,
                      r.settlingMechanical, r.lateSpanMechanical, r.earlySamples, r.lateSamples);
    }
    Serial.printf("Maximum absolute settling: %.4f mechanical deg\n", maxSettling);
    Serial.printf("Maximum late-window span: %.4f mechanical deg\n", maxLateSpan);
    Serial.printf("Profile rows: %u/%u\n", profileCount, MAX_PROFILE_ROWS);
    Serial.println("=== END OFFSET PROFILE ===");
}

float wrapAngle(float a) {
    while (a > PI_F) a -= TAU_F;
    while (a < -PI_F) a += TAU_F;
    return a;
}

float positiveAngle(float a) {
    a = fmodf(a, TAU_F);
    return a < 0 ? a + TAU_F : a;
}

uint8_t crc6(uint32_t data) {
    uint8_t crc = 0;
    for (int bit = 17; bit >= 0; --bit) {
        bool feedback = ((crc >> 5) ^ (data >> bit)) & 1U;
        crc = (crc << 1) & 0x3F;
        if (feedback) crc ^= 3;
    }
    return crc;
}

// Hardware SPI mode 1, 4 MHz and 24-bit frames follow the upstream MT6701
// transport. Unlike upstream, TRACK LOST samples do not update the angle.
bool readSensor() {
    sampleGood = false;
    sensorLost = false;
    spi_transaction_t transaction = {};
    transaction.flags = SPI_TRANS_USE_RXDATA;
    transaction.length = 24;
    transaction.rxlength = 24;
    if (spi_device_polling_transmit(sensorSpi, &transaction) != ESP_OK) {
        sampleError = "SPI transfer failed";
        return false;
    }
    uint32_t frame = (uint32_t(transaction.rx_data[0]) << 16)
                   | (uint32_t(transaction.rx_data[1]) << 8)
                   | transaction.rx_data[2];
    ++samplesRead;
    if (frame == 0 || frame == 0xFFFFFFUL) {
        sampleError = "Constant-level sensor frame";
        return false;
    }
    if (crc6(frame >> 6) != (frame & 63)) {
        ++crcErrors;
        sampleError = "Sensor CRC error";
        return false;
    }
    fieldStatus = (frame >> 6) & 3;
    if (fieldStatus == 2) ++weakFrames;
    if (fieldStatus == 1 || fieldStatus == 3) {
        sampleError = "Strong field or reserved field status";
        return false;
    }
    if (frame & (1UL << 9)) {
        ++lostFrames;
        sensorLost = true;
        sampleError = "TRACK LOST";
        if (!gapOpen) { gapOpen = true; gapStartUs = micros(); }
        return false;
    }
    rawAngle = (frame >> 10) * (TAU_F / 16384);
    // Circular low-pass filtering, alpha 0.4, as in the upstream sensor.
    float x = cosf(rawAngle), y = sinf(rawAngle);
    if (!haveAngle) { filterX = x; filterY = y; }
    else { filterX += 0.4f * (x - filterX); filterY += 0.4f * (y - filterY); }
    filteredAngle = positiveAngle(atan2f(filterY, filterX));
    if (haveAngle) continuousAngle += wrapAngle(filteredAngle - previousFiltered);
    else continuousAngle = filteredAngle;
    previousFiltered = filteredAngle;
    haveAngle = true;
    lastGoodUs = micros();
    if (gapOpen) {
        uint32_t gap = lastGoodUs - gapStartUs;
        if (gap > longestGapUs) longestGapUs = gap;
        gapOpen = false;
    }
    sampleGood = true;
    sampleError = "OK";
    return true;
}

void stopTest(const char* reason) {
    if (ready) motor.disable();
    stage = Stage::Off;
    Serial.print("STOP: "); Serial.println(reason);
    if (profileCount) { printProfile(); profileCount = 0; }
}

void beginStage(Stage next) {
    stage = next;
    stageStartedMs = millis();
    averageSum = 0;
    averageCount = 0;
    earlySum = 0; earlyCount = 0;
    lateMin = 1e9f; lateMax = -1e9f;
}

void moveTo(float angle, Stage next) {
    destination = angle;
    beginStage(next);
}

bool moveComplete(float dt) {
    float delta = destination - commandAngle;
    float step = ELECTRICAL_SPEED * dt;
    commandAngle += constrain(delta, -step, step);
    return fabsf(destination - commandAngle) < 0.0001f;
}

bool settled(uint32_t elapsed, uint32_t duration) {
    // Average accepted samples from the final 100 ms of each hold.
    if (elapsed >= duration - 100 && sampleGood) {
        averageSum += continuousAngle;
        ++averageCount;
    }
    if (elapsed < duration) return false;
    if (averageCount < 30) {
        stopTest("Too few valid samples at the measurement endpoint");
        return false;
    }
    return true;
}

void finishCalibration() {
    // Disable all phases before producing the longer result report.
    motor.disable();
    stage = Stage::Off;
    float a = atan2f(offsetY[0], offsetX[0]);
    float b = atan2f(offsetY[1], offsetX[1]);
    float agreement = fabsf(wrapAngle(a - b));
    float coherenceA = hypotf(offsetX[0], offsetY[0]) / offsetCount[0];
    float coherenceB = hypotf(offsetX[1], offsetY[1]) / offsetCount[1];
    float mean = atan2f(offsetY[0] + offsetY[1], offsetX[0] + offsetX[1]);
    // SimpleFOC's positive q-axis voltage shifts the field by pi/2.
    // This is the same offset convention as Scott's calibration.
    float electricalZero = positiveAngle(mean + 1.5f * PI_F);
    Serial.printf("Pole pairs: %d | Forward estimate: %.3f | Reverse estimate: %.3f\n",
                  measuredPairs, forwardEstimate, reverseEstimate);
    Serial.printf("Direction: %s | Offset agreement: %.2f electrical deg\n",
                  measuredDirection > 0 ? "CW" : "CCW", agreement * RAD_TO_DEGREES);
    Serial.printf("Offset coherence: %.3f / %.3f | Electrical zero: %.6f rad\n",
                  coherenceA, coherenceB, electricalZero);
    Serial.printf("Samples: %lu | CRC errors: %lu | TRACK LOST: %lu | Weak field: %lu\n",
                  (unsigned long)samplesRead, (unsigned long)crcErrors,
                  (unsigned long)lostFrames, (unsigned long)weakFrames);
    if (agreement > 15 / RAD_TO_DEGREES || coherenceA < 0.98f || coherenceB < 0.98f) {
        Serial.println("RESULT: FAIL - electrical offset is not repeatable enough.");
    } else {
        Serial.println("RESULT: CALIBRATION CONSISTENT - repeat to check reproducibility.");
        Serial.println("Weak-field warnings still prevent a claim of verified angle accuracy.");
    }
    printProfile();
    Serial.println("Motor OFF. Values not saved. Spring remains disabled in v6.");
    profileCount = 0;
}

void calibrationTick(float dt) {
    uint32_t elapsed = millis() - stageStartedMs;
    switch (stage) {
    case Stage::Align:
        motor.voltage_limit = TEST_VOLTAGE * min(1.0f, elapsed / 500.0f);
        if (settled(elapsed, 1500)) {
            directionStart = averageSum / averageCount;
            moveTo(3 * TAU_F, Stage::DirectionMove);
        }
        break;
    case Stage::DirectionMove:
        if (moveComplete(dt)) beginStage(Stage::DirectionHold);
        break;
    case Stage::DirectionHold:
        if (settled(elapsed, 1000)) {
            float change = averageSum / averageCount - directionStart;
            // Broader than upstream's 180-degree maximum: do not reject a
            // four-pole-pair motor before actually measuring its pole count.
            if (fabsf(change) < 30 / RAD_TO_DEGREES || fabsf(change) > TAU_F + 0.2f) {
                stopTest("Direction test: unexpected mechanical travel");
                Serial.printf("Travel: %.2f deg\n", change * RAD_TO_DEGREES);
                return;
            }
            measuredDirection = change > 0 ? 1 : -1;
            moveTo(commandAngle + TAU_F, Stage::PreMove);
        }
        break;
    case Stage::PreMove:
        if (moveComplete(dt)) beginStage(Stage::PreHold);
        break;
    case Stage::PreHold:
        if (settled(elapsed, 1000)) {
            mechanicalStart = averageSum / averageCount;
            moveTo(commandAngle + MEASUREMENT_TURNS * TAU_F, Stage::Forward);
        }
        break;
    case Stage::Forward:
        if (moveComplete(dt)) beginStage(Stage::ForwardHold);
        break;
    case Stage::ForwardHold:
        if (settled(elapsed, 1000)) {
            mechanicalEnd = averageSum / averageCount;
            float travel = measuredDirection * (mechanicalEnd - mechanicalStart);
            forwardEstimate = travel > 0.01f ? MEASUREMENT_TURNS * TAU_F / travel : 0;
            measuredPairs = lroundf(forwardEstimate);
            if (measuredPairs < 3 || measuredPairs > 12 ||
                fabsf(forwardEstimate - measuredPairs) > 0.20f) {
                stopTest("Forward pole-pair measurement is not consistent");
                Serial.printf("Forward estimate: %.3f | Travel: %.2f deg\n",
                              forwardEstimate, travel * RAD_TO_DEGREES);
                return;
            }
            moveTo(commandAngle - MEASUREMENT_TURNS * TAU_F, Stage::Reverse);
        }
        break;
    case Stage::Reverse:
        if (moveComplete(dt)) beginStage(Stage::ReverseHold);
        break;
    case Stage::ReverseHold:
        if (settled(elapsed, 1000)) {
            float returned = averageSum / averageCount;
            float travel = measuredDirection * (mechanicalEnd - returned);
            reverseEstimate = travel > 0.01f ? MEASUREMENT_TURNS * TAU_F / travel : 0;
            float returnError = fabsf(returned - mechanicalStart);
            if (fabsf(reverseEstimate - measuredPairs) > 0.20f ||
                fabsf(forwardEstimate - reverseEstimate) > 0.15f ||
                returnError > 6 / RAD_TO_DEGREES) {
                stopTest("Forward/reverse consistency check failed");
                Serial.printf("Forward: %.3f | Reverse: %.3f | Return error: %.2f deg\n",
                              forwardEstimate, reverseEstimate, returnError * RAD_TO_DEGREES);
                return;
            }
            offsetBase = commandAngle;
            offsetSteps = PROFILE_INTERVALS; // Full mechanical turn, not a half-turn.
            offsetStep = offsetPass = 0;
            offsetX[0] = offsetX[1] = offsetY[0] = offsetY[1] = 0;
            offsetCount[0] = offsetCount[1] = 0;
            beginStage(Stage::OffsetHold);
        }
        break;
    case Stage::OffsetMove:
        if (moveComplete(dt)) beginStage(Stage::OffsetHold);
        break;
    case Stage::OffsetHold:
        if (sampleGood && elapsed >= 100 && elapsed < 200) {
            earlySum += continuousAngle;
            ++earlyCount;
        }
        if (sampleGood && elapsed >= 700) {
            lateMin = min(lateMin, continuousAngle);
            lateMax = max(lateMax, continuousAngle);
        }
        if (settled(elapsed, 800)) {
            if (earlyCount < 30 || profileCount >= MAX_PROFILE_ROWS) {
                stopTest("Insufficient early samples or profile buffer full");
                return;
            }
            float sensorElectrical = measuredDirection * measuredPairs * (averageSum / averageCount);
            float difference = wrapAngle(sensorElectrical - positiveAngle(commandAngle));
            const float settling = (averageSum / averageCount - earlySum / earlyCount) * RAD_TO_DEGREES;
            const float lateSpan = (lateMax - lateMin) * RAD_TO_DEGREES;
            maxSettling = max(maxSettling, fabsf(settling));
            maxLateSpan = max(maxLateSpan, lateSpan);
            profile[profileCount++] = {
                offsetPass, offsetStep, (commandAngle - offsetBase) * RAD_TO_DEGREES,
                positiveAngle(averageSum / averageCount) * RAD_TO_DEGREES,
                difference * RAD_TO_DEGREES, settling, lateSpan, earlyCount, averageCount
            };
            offsetX[offsetPass] += cosf(difference);
            offsetY[offsetPass] += sinf(difference);
            ++offsetCount[offsetPass];
            if (offsetPass == 0 && offsetStep < offsetSteps) ++offsetStep;
            else if (offsetPass == 0) { offsetPass = 1; beginStage(Stage::OffsetHold); break; }
            else if (offsetStep > 0) --offsetStep;
            else { finishCalibration(); return; }
            moveTo(offsetBase + offsetStep * (measuredPairs * TAU_F / PROFILE_INTERVALS), Stage::OffsetMove);
        }
        break;
    case Stage::Off: return;
    }
    if (stage != Stage::Off) motor.move(commandAngle);
}

void command(char key) {
    if (key == 'x' || key == 'X') { stopTest("User command"); return; }
    if (key == 's') { Serial.println("Spring disabled: calibration-only version."); return; }
    if (key != 'c' || stage != Stage::Off) return;
    if (!ready || !sampleGood || micros() - lastGoodUs >= SENSOR_TIMEOUT_US || digitalRead(DRIVER_DIAG)) {
        Serial.println("START BLOCKED: check sensor or driver status."); return;
    }
    Serial.println("CALIBRATION v6: release rotor. About 2 minutes; x stops.");
    Serial.println("3 turns for direction, 20 forward + 20 reverse, then full-turn offset profiling.");
    Serial.printf("Motor voltage: %.2f V. Short TRACK LOST samples skipped; motion stays open-loop.\n", TEST_VOLTAGE);
    samplesRead = crcErrors = lostFrames = weakFrames = longestGapUs = 0;
    profileCount = 0; maxSettling = maxLateSpan = 0;
    commandAngle = 0;
    motor.shaft_angle = 0;
    motor.target = 0;
    motor.voltage_limit = 0;
    motor.enable();
    testStartedMs = millis();
    previousTickUs = micros();
    beginStage(Stage::Align);
}

void setup() {
    for (uint8_t pin : MOTOR_PINS) { pinMode(pin, OUTPUT); digitalWrite(pin, LOW); }
    pinMode(BACKLIGHT, OUTPUT); digitalWrite(BACKLIGHT, LOW);
    pinMode(DRIVER_DIAG, INPUT);
    Serial.begin(115200);
    delay(2500);
    Serial.println("\n=== SmartKnob v6 - upstream-based calibration only ===");
    spi_bus_config_t bus = {};
    bus.mosi_io_num = -1; bus.miso_io_num = SENSOR_DATA; bus.sclk_io_num = SENSOR_CLK;
    bus.quadwp_io_num = -1; bus.quadhd_io_num = -1; bus.max_transfer_sz = 4;
    spi_device_interface_config_t device = {};
    device.mode = 1; device.clock_speed_hz = 4000000; device.spics_io_num = SENSOR_CS;
    device.cs_ena_pretrans = 4; device.queue_size = 1;
    if (spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO) != ESP_OK ||
        spi_bus_add_device(SPI2_HOST, &device, &sensorSpi) != ESP_OK) {
        Serial.println("FAIL: sensor SPI initialization"); return;
    }
    driver.voltage_power_supply = 5;
    driver.pwm_frequency = 20000;
    driver.dead_zone = 0.02f;
    if (!driver.init()) { Serial.println("FAIL: 6-PWM initialization"); return; }
    motor.linkDriver(&driver);
    motor.controller = MotionControlType::angle_openloop;
    motor.torque_controller = TorqueControlType::voltage;
    motor.foc_modulation = FOCModulationType::SinePWM;
    motor.sensor_direction = Direction::CW;
    motor.zero_electric_angle = 0;
    motor.velocity_limit = 2 * ELECTRICAL_SPEED;
    motor.voltage_limit = 0;
    motor.init(); // Zero differential voltage during initialization.
    motor.disable();
    ready = true;
    previousTickUs = micros();
    Serial.println("Motor OFF. c = calibrate | x = stop. s is disabled.");
}

void loop() {
    if (!ready) { delay(100); return; }
    static uint32_t lastReport = 0;
    while (Serial.available()) command(Serial.read());
    uint32_t now = micros();
    uint32_t gap = now - previousTickUs;
    if (gap < 1000) return;
    previousTickUs = now;
    bool active = stage != Stage::Off;
    if (active && gap > 20000) { stopTest("Control loop delayed more than 20 ms"); return; }
    bool good = readSensor();
    if (active) {
        if (digitalRead(DRIVER_DIAG)) { stopTest("Motor driver fault"); return; }
        if (!good && !sensorLost) { stopTest(sampleError); return; }
        // Calibration is open-loop: no rejected angle enters commutation.
        // Brief lost frames do NOT disable/re-enable the driver repeatedly.
        if ((!good && micros() - lastGoodUs >= SENSOR_TIMEOUT_US) ||
            longestGapUs >= SENSOR_TIMEOUT_US) {
            stopTest("Sensor unavailable for 5 ms or more"); return;
        }
        if (millis() - testStartedMs > CALIBRATION_TIMEOUT_MS) {
            stopTest("Calibration time limit"); return;
        }
        calibrationTick(gap * 1e-6f);
    } else if (millis() - lastReport >= 2000) {
        lastReport = millis();
        Serial.printf("OFF v6 | Angle: %.2f deg | Sample: %s | Field: %s | CRC: %lu | Lost: %lu\n",
                      filteredAngle * RAD_TO_DEGREES, sampleError,
                      fieldStatus == 2 ? "TOO WEAK" : (fieldStatus == 0 ? "OK" : "INVALID"),
                      (unsigned long)crcErrors, (unsigned long)lostFrames);
    }
}

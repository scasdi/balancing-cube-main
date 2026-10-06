/**
 * @file main.cpp
 * @brief Bring-up test for the B-G431B-ESC1: is the serial link alive, does the
 *        6-PWM driver initialise, and does the motor turn?
 *
 * Open-loop velocity only. No position sensor, no FOC, no current control. This
 * proves the toolchain, the serial link and the motor wiring and nothing else -
 * it is not a step toward the real driver firmware.
 *
 * HOW TO READ THE OUTPUT, because each case points somewhere different:
 *
 *   Nothing at all                -> the link to the PC is broken. Wrong COM
 *                                    port, or the upload did not actually land.
 *                                    Not a motor problem.
 *   Banner but no heartbeat       -> serial works and the chip runs, but
 *                                    driver.init() hung inside the library.
 *   Heartbeat with init=0         -> init() returned failure. The power stage is
 *                                    never switched, so the motor cannot move.
 *   Heartbeat with init=1         -> firmware is driving. If the shaft still
 *                                    does not turn, the fault is now electrical:
 *                                    bus supply, phase wiring, or current limit.
 *
 * STAGE 1 runs on USB alone - no bench supply, no motor. driver.init() only
 * configures TIM1 and the pin modes, so it reports the same result with the
 * power stage unpowered. Confirm serial and init=1 here first.
 *
 * STAGE 2 adds the bench supply and the motor, and the instrument is the
 * supply's ammeter rather than this text: the logic is fed from USB, so a dead
 * power stage draws essentially nothing from the bus, while a live one draws
 * real current that changes when the motor is unplugged.
 *
 * THE SAFETY ARGUMENT, because the numbers here are not arbitrary:
 *
 *   Phase-to-phase resistance measured 10 ohm. The winding is wye, so each phase
 *   is half of that. In open loop at low speed the rotor generates almost no
 *   back-EMF, which means nothing opposes the applied voltage and the current is
 *   set by Ohm's law alone:
 *
 *       I = VOLTAGE_LIMIT / PHASE_RESISTANCE = 2.5 / 5.0 = 0.5 A
 *
 *   That is the entire protection. Current scales directly with VOLTAGE_LIMIT,
 *   and a rotor that loses sync and stalls draws it continuously rather than in
 *   bursts. Do not raise VOLTAGE_LIMIT without recomputing this line.
 *
 *   The bench supply's current limit is the backstop, not the plan.
 */

#include <Arduino.h>
#include <SimpleFOC.h>

// Measured 10 ohm across any two motor leads; wye, so each phase is half.
static const float PHASE_RESISTANCE_OHM = 5.0f;

// Phase current, and the only knob you should turn here.
//
// No measurement justifies any particular value yet. The earlier attempts at
// 0.2, 0.5 and 0.8 A all ran while driver.init() was returning 0, so no field
// ever rotated and none of them say anything about breakaway torque. 0.5 A is
// just a conservative starting point.
//
// If the rotor will not turn with init=1 and voltage confirmed on the phases,
// go to 0.8 A, then stop and tell me rather than climbing further. A GM4108H is a gimbal motor and its continuous rating
// is around 1 A; past that you are heating the winding to prove a point.
static const float TARGET_CURRENT_A = 0.5f;

static const float VOLTAGE_LIMIT = PHASE_RESISTANCE_OHM * TARGET_CURRENT_A;  // 2.5 V

// The bench supply setting, and it must match the supply's actual output: every
// duty SimpleFOC computes is U / voltage_power_supply, so a mismatch here scales
// every phase voltage wrongly.
//
// Dropped from 24 V to 6 V for the shoot-through hunt. If both FETs of a bridge
// conduct at once, the bus is shorted through them and the current is set by the
// bus voltage against their on-resistance - so a quarter of the voltage is a
// quarter of the fault current, and the test becomes survivable.
static const float SUPPLY_VOLTAGE = 6.0f;

// Unknown for the GM4108H. In open loop this only scales speed - it never
// affects current - so a wrong guess is harmless. The test measures the real
// value; see MEASURING POLE PAIRS at the end of this file.
static const int POLE_PAIRS_ASSUMED = 11;

// Deliberately crawling, and the reason is the multimeter's bandwidth.
//
// Open loop spins the FIELD at (commanded velocity x pole pairs) electrical
// rad/s, so 2 rad/s mechanical is 22 rad/s electrical - about 3.5 Hz. A DMM
// averages over its sampling window, so at 3.5 Hz it reports the modulation
// centre as one steady number and the switching is invisible. At 0.05 rad/s the
// electrical period stretches to ~11 s and each phase voltage sweeps its full
// range slowly enough to watch on the display.
//
// Raise this to 2.0 once a motor is attached: one mechanical revolution takes
// over two minutes at this speed, which is useless for timing the rotor.
static const float TARGET_VELOCITY_RADS = 0.05f;

// ONE PHASE PER BUILD - localises the shoot-through without serial.
//
// Driving all three phases proved current flows with no motor attached, so it
// circulates inside the board, but not where. Energising one bridge at a time
// separates the three explanations:
//
//   current on any single phase    -> systemic, i.e. the dead-time configuration.
//                                     Most likely SimpleFOC fell back to software
//                                     6-PWM, where the high and low channels are
//                                     separate and need not be phase-aligned, so
//                                     they can overlap.
//   current on one phase only      -> that half-bridge is faulty.
//   current only with two or more  -> a path BETWEEN phase pads. Not
//                                     shoot-through at all.
//
// The selection is compile-time rather than a timed sweep because a sweep needs
// serial and the bus powered together to attribute a reading to a phase, and
// that combination is not happening without a USB isolator. So: flash, confirm
// over USB which phase this build drives, unplug USB, power the bus, read the
// ammeter. No timing and no correlation to get wrong.
//
//   0 = phase A    1 = phase B    2 = phase C    3 = none (baseline)
static const int TEST_PHASE = 0;

static_assert(TEST_PHASE >= 0 && TEST_PHASE <= 3, "TEST_PHASE must be 0, 1, 2 or 3");

static const char* const PHASE_NAME[4] = { "phase A", "phase B", "phase C", "nothing (baseline)" };

// A commanded duty below dead_zone produces no high-side conduction whatsoever -
// the pulse is swallowed by the enforced both-off time. That is what made the
// previous run's 0.3 V phase read zero and look broken. At a 6 V bus the floor is
// 0.02 x 6 = 0.12 V, so 2.0 V (33% duty) clears it by a wide margin. It also
// stays under VOLTAGE_LIMIT, which setPwm() would otherwise clamp it to.
static const float TEST_DRIVE_V = 2.0f;

// Long enough for `pio run -t upload -t monitor` to attach before the banner is
// printed. The upload resets the board, so without this the banner is emitted
// into a port nobody is listening on yet and the first evidence is lost.
static const uint32_t STARTUP_DELAY_MS = 2500;

// Often enough to read as motion on screen, rare enough not to flood the port.
static const uint32_t HEARTBEAT_MS = 500;

BLDCMotor      motor  = BLDCMotor(POLE_PAIRS_ASSUMED);
BLDCDriver6PWM driver = BLDCDriver6PWM(A_PHASE_UH, A_PHASE_UL,
                                       A_PHASE_VH, A_PHASE_VL,
                                       A_PHASE_WH, A_PHASE_WL);

// -1 means driver.init() was never reached. Distinguishing that from a returned
// 0 is the whole point: one is a hang, the other is a reported failure.
static int driver_init_result = -1;

void setup() {
    Serial.begin(115200);
    delay(STARTUP_DELAY_MS);

    Serial.println();
    Serial.println("=== B-G431B-ESC1 bring-up ===");
    Serial.println("serial link OK - this line proves TX reaches the PC");
    Serial.println("type any character to test the RX direction");

    driver.voltage_power_supply = SUPPLY_VOLTAGE;
    driver.voltage_limit        = VOLTAGE_LIMIT;   // Hard clamp at the driver too

    // Printed before the call, so a hang inside the library leaves the banner on
    // screen with no result line after it.
    Serial.println("calling driver.init() ...");
    driver_init_result = driver.init();
    Serial.print("driver.init() returned ");
    Serial.println(driver_init_result);

    if (driver_init_result != 1) {
        Serial.println("power stage NOT configured - the motor cannot move");
        return;
    }

    motor.linkDriver(&driver);

    // Open loop: there is no sensor, so initFOC() and loopFOC() are deliberately
    // absent. Calling them without a sensor is the usual reason this board sits
    // still.
    motor.controller     = MotionControlType::velocity_openloop;
    motor.voltage_limit  = VOLTAGE_LIMIT;
    motor.velocity_limit = TARGET_VELOCITY_RADS;
    motor.init();

    // The library's actual dead-time settings, read back rather than assumed.
    //
    // SimpleFOC computes dead_time_ns = (1e9 / pwm_frequency) * dead_zone and
    // programs it into TIM1's BDTR register. Printing the inputs gives the real
    // number instead of a guess, and gives the duty floor below which a
    // commanded voltage produces no high-side conduction at all - the trap that
    // made an earlier 0.3 V test phase read as a dead bridge.
    const float dead_time_ns   = (1e9f / (float)driver.pwm_frequency) * driver.dead_zone;
    const float duty_floor_v   = driver.dead_zone * SUPPLY_VOLTAGE;
    const float commanded_duty = TEST_DRIVE_V / SUPPLY_VOLTAGE;

    Serial.print("pwm_frequency = ");
    Serial.print(driver.pwm_frequency);
    Serial.println(" Hz");
    Serial.print("dead_zone = ");
    Serial.print(driver.dead_zone, 4);
    Serial.print(" of the period = ");
    Serial.print(dead_time_ns, 0);
    Serial.println(" ns");
    Serial.print("duty floor = ");
    Serial.print(duty_floor_v, 3);
    Serial.println("V (anything below this never switches the high side)");
    Serial.print("commanded duty = ");
    Serial.print(commanded_duty * 100.0f, 1);
    Serial.println("%");

    // Printed while USB is still the only connection, so the build can be
    // confirmed before the cable comes out and the bus goes live.
    Serial.print("THIS BUILD DRIVES: ");
    Serial.print(PHASE_NAME[TEST_PHASE]);
    Serial.print(" at ");
    Serial.print(TEST_DRIVE_V);
    Serial.print("V, bus expected at ");
    Serial.print(SUPPLY_VOLTAGE);
    Serial.println("V");
}

void loop() {
    // motor.move() is deliberately NOT called: the motion layer is proven innocent
    // and leaving it out keeps the measurement unambiguous.
    // TEST_PHASE is a compile-time constant, so this folds to one fixed call.
    const float ua = (TEST_PHASE == 0) ? TEST_DRIVE_V : 0.0f;
    const float ub = (TEST_PHASE == 1) ? TEST_DRIVE_V : 0.0f;
    const float uc = (TEST_PHASE == 2) ? TEST_DRIVE_V : 0.0f;

    if (driver_init_result == 1) driver.setPwm(ua, ub, uc);

    // Echo whatever arrives, which is the only way to confirm the PC-to-board
    // direction. The command interface will need it working later anyway.
    while (Serial.available()) {
        Serial.print("rx: ");
        Serial.println((char)Serial.read());
    }

    static uint32_t last_beat_ms = 0;
    if (millis() - last_beat_ms < HEARTBEAT_MS) return;
    last_beat_ms = millis();

    // A climbing counter separates "running" from "printed once and froze", and
    // repeating the state means the monitor can be opened at any moment and
    // still show what is going on.
    static uint32_t beat = 0;
    beat++;

    Serial.print("[");
    Serial.print(beat);
    Serial.print("] t=");
    Serial.print(millis());
    Serial.print("ms init=");
    Serial.print(driver_init_result);

    if (driver_init_result != 1) {
        Serial.println(" NOT DRIVING");
        return;
    }

    Serial.print(" driving ");
    Serial.print(PHASE_NAME[TEST_PHASE]);
    Serial.print(" at ");
    Serial.print(TEST_DRIVE_V);
    Serial.println("V");
}

/*
 * MEASURING POLE PAIRS
 *
 * Open loop spins the field at (commanded velocity x configured pole pairs)
 * electrical rad/s. The rotor follows at that rate divided by its real pole
 * pair count, so:
 *
 *     measured_speed = commanded x (POLE_PAIRS_ASSUMED / pole_pairs_actual)
 *
 * Rearranged, timing one full mechanical revolution gives the real value:
 *
 *     pole_pairs_actual = POLE_PAIRS_ASSUMED x commanded / measured
 *                       = 11 x 2.0 / (2*PI / seconds_per_revolution)
 *
 * With 11 assumed and TARGET_VELOCITY_RADS set back to 2.0, a revolution takes
 * 3.1 s if 11 is correct. Twice that means half the pole pairs, and so on.
 */

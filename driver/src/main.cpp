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

// The bench supply setting. SimpleFOC needs it to scale its PWM duty.
static const float SUPPLY_VOLTAGE = 24.0f;

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

// STATIC DUTY TEST - three fixed voltages, one per phase.
//
// This bypasses motor.move() and everything under it: velocityOpenloop(), the
// electrical angle, the sine tables, the sensor-direction defaults. It talks to
// driver.setPwm() directly, which is the last software step before the timer
// registers. If the pads stay at 0 V with this, the fault is in the timer or the
// power stage, and no amount of motion-layer debugging will find it.
//
// setPwm() clamps each argument to voltage_limit, then computes
// duty = U / voltage_power_supply. The pad's DC average is duty x bus voltage,
// which is U again - so each pad should measure the number commanded here.
//
// Three different values, because three identical ones could be confused with a
// common-mode offset or a probe on the wrong pad.
static const float TEST_UA = 2.5f;
static const float TEST_UB = 1.2f;
static const float TEST_UC = 0.3f;

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

    Serial.println("open-loop velocity control active");
}

void loop() {
    // motor.move() is deliberately NOT called: this is the reduced test, and the
    // whole point is to leave the motion layer out of the measurement. The timer
    // registers hold their value, so re-applying every iteration is harmless.
    if (driver_init_result == 1) driver.setPwm(TEST_UA, TEST_UB, TEST_UC);

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

    Serial.print(" static setPwm(");
    Serial.print(TEST_UA);
    Serial.print(", ");
    Serial.print(TEST_UB);
    Serial.print(", ");
    Serial.print(TEST_UC);
    Serial.println(") - measure these volts DC at the three phase pads");
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

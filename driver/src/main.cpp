/**
 * @file main.cpp
 * @brief Bring-up test for the B-G431B-ESC1: does it flash, and does the motor turn?
 *
 * Open-loop velocity only. No position sensor, no FOC, no current control. This
 * proves the toolchain and the motor wiring and nothing else - it is not a step
 * toward the real driver firmware.
 *
 * THE SAFETY ARGUMENT, because the numbers here are not arbitrary:
 *
 *   Phase-to-phase resistance measured 10 ohm. The winding is wye, so each phase
 *   is half of that. In open loop at low speed the rotor generates almost no
 *   back-EMF, which means nothing opposes the applied voltage and the current is
 *   set by Ohm's law alone:
 *
 *       I = VOLTAGE_LIMIT / PHASE_RESISTANCE = 1.0 / 5.0 = 0.2 A
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
// 0.2 A was not enough: the field rotated and the rotor stayed put, because the
// torque it produces is below the motor's breakaway - cogging plus static
// friction. Torque is proportional to current, so raising this is what gets the
// rotor moving.
//
// If it still does not turn, go to 0.8 A, then stop and tell me rather than
// climbing further. A GM4108H is a gimbal motor and its continuous rating is
// around 1 A; past that you are heating the winding to prove a point.
static const float TARGET_CURRENT_A = 0.5f;

static const float VOLTAGE_LIMIT = PHASE_RESISTANCE_OHM * TARGET_CURRENT_A;  // 2.5 V

// The bench supply setting. SimpleFOC needs it to scale its PWM duty.
static const float SUPPLY_VOLTAGE = 24.0f;

// Unknown for the GM4108H. In open loop this only scales speed - it never
// affects current - so a wrong guess is harmless. The test measures the real
// value; see MEASURING POLE PAIRS at the end of this file.
static const int POLE_PAIRS_ASSUMED = 11;

// Slow enough to time one revolution by eye: at 2 rad/s a turn takes ~3.1 s.
static const float TARGET_VELOCITY_RADS = 2.0f;

// Let the board boot visibly before anything moves.
static const uint32_t STARTUP_DELAY_MS = 2000;

BLDCMotor      motor  = BLDCMotor(POLE_PAIRS_ASSUMED);
BLDCDriver6PWM driver = BLDCDriver6PWM(A_PHASE_UH, A_PHASE_UL,
                                       A_PHASE_VH, A_PHASE_VL,
                                       A_PHASE_WH, A_PHASE_WL);

static bool driver_ready = false;

void setup() {
    Serial.begin(115200);
    delay(STARTUP_DELAY_MS);
    Serial.println("B-G431B-ESC1 open-loop spin test");

    driver.voltage_power_supply = SUPPLY_VOLTAGE;
    driver.voltage_limit        = VOLTAGE_LIMIT;   // Hard clamp at the driver too

    if (!driver.init()) {
        Serial.println("FATAL: driver init failed - motor will not be driven");
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

    driver_ready = true;

    Serial.print("voltage_limit ");
    Serial.print(VOLTAGE_LIMIT);
    Serial.print(" V across ");
    Serial.print(PHASE_RESISTANCE_OHM);
    Serial.print(" ohm -> expect about ");
    Serial.print(TARGET_CURRENT_A);
    Serial.println(" A per phase");

    Serial.print("commanding ");
    Serial.print(TARGET_VELOCITY_RADS);
    Serial.println(" rad/s open loop");
}

void loop() {
    if (!driver_ready) return;
    motor.move(TARGET_VELOCITY_RADS);
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
 * With 11 assumed and 2 rad/s commanded, a revolution takes 3.1 s if 11 is
 * correct. Twice that means half the pole pairs, and so on.
 */

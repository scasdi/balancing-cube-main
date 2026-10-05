#!/usr/bin/env python3
"""Generate hardware/cube.kicad_sch for the 2D reaction-wheel cube prototype.

The schematic is generated rather than drawn so it stays consistent with
include/ESP/pins.h: edit PINS below, re-run, and the sheet follows.

Connectivity is expressed with global labels on every pin instead of routed
wires. Electrically that is identical - KiCad joins nets by label name - and it
keeps the sheet readable and this generator free of routing geometry.

What a schematic cannot express: every GND here is one net with zero impedance.
The star topology, wire gauges and physical placement live in the separate
harness drawing; the notes block points at that.

    python3 hardware/gen_schematic.py
"""

VERSION = "20231120"
PAPER = "A3"          # 420 x 297 mm

_uid = 0
def uid() -> str:
    """Deterministic UUIDs, so regenerating gives a clean diff."""
    global _uid
    _uid += 1
    h = f"{_uid:032x}"
    return f"{h[0:8]}-{h[8:12]}-{h[12:16]}-{h[16:20]}-{h[20:32]}"


# --- component library ------------------------------------------------------
# (ref, value, footprint-ish description, [(pin_no, pin_name, type, side)])
#   side: "L" left, "R" right.   type: passive/power_in/power_out/input/output/
#                                      bidirectional/no_connect

PARTS = [
    ("BT1", "6S LiPo", "22.2 V nom / 25.2 V max", [
        ("1", "+",  "power_out", "R"),
        ("2", "-",  "power_out", "R"),
    ]),

    ("J1", "PDB", "integrated 12 V and 5 V bucks", [
        ("1", "VBAT_IN+", "power_in",  "L"),
        ("2", "VBAT_IN-", "power_in",  "L"),
        ("3", "VBAT_OUT", "power_out", "R"),
        ("4", "12V",      "power_out", "R"),
        ("5", "5V",       "power_out", "R"),
        ("6", "GND",      "power_out", "R"),
    ]),

    ("F1", "3A slow-blow", "blade fuse + holder", [
        ("1", "1", "passive", "L"),
        ("2", "2", "passive", "R"),
    ]),

    ("C1", "1000uF 50V", "electrolytic - MOUNT AT A1 PADS", [
        ("1", "+", "passive", "L"),
        ("2", "-", "passive", "R"),
    ]),

    ("A1", "B-G431B-ESC1", "STM32G431CB 3-shunt FOC", [
        ("1", "V+",   "power_in",     "L"),
        ("2", "V-",   "power_in",     "L"),
        ("3", "5V",   "power_out",    "L"),
        ("4", "TX",   "output",       "L"),
        ("5", "RX",   "input",        "L"),
        ("6", "PWM",  "input",        "L"),
        ("7", "GND",  "power_in",     "L"),
        ("8", "OUT1", "power_out",    "R"),
        ("9", "OUT2", "power_out",    "R"),
        ("10","OUT3", "power_out",    "R"),
    ]),

    ("M1", "GM4108H", "gimbal motor, 3 phase", [
        ("1", "A", "passive", "L"),
        ("2", "B", "passive", "L"),
        ("3", "C", "passive", "L"),
    ]),

    ("U1", "ESP32-DevKitC", "pins in use only", [
        ("1",  "5V",     "power_in",     "L"),
        ("2",  "GND",    "power_in",     "L"),
        ("3",  "3V3",    "power_out",    "L"),
        ("4",  "GPIO17", "output",       "R"),
        ("5",  "GPIO16", "input",        "R"),
        ("6",  "GPIO21", "bidirectional","R"),
        ("7",  "GPIO22", "bidirectional","R"),
        ("8",  "GPIO13", "output",       "R"),
        ("9",  "GPIO36", "input",        "R"),
        ("10", "GPIO23", "output",       "R"),
        ("11", "GPIO2",  "output",       "R"),
        ("12", "GPIO4",  "output",       "R"),
        ("13", "GPIO5",  "output",       "R"),
    ]),

    ("U2", "BNO085", "IMU breakout - leads under 10 cm", [
        ("1", "VIN", "power_in",     "L"),
        ("2", "GND", "power_in",     "L"),
        ("3", "SDA", "bidirectional","R"),
        ("4", "SCL", "bidirectional","R"),
    ]),

    ("U3", "Servo buck", "12 V to 5-6 V", [
        ("1", "IN+",  "power_in",  "L"),
        ("2", "IN-",  "power_in",  "L"),
        ("3", "OUT+", "power_out", "R"),
        ("4", "OUT-", "power_out", "R"),
    ]),

    ("M2", "Servo", "3-wire lead", [
        ("1", "V+",  "power_in", "L"),
        ("2", "GND", "power_in", "L"),
        ("3", "SIG", "input",    "L"),
    ]),

    ("SW1", "Button", "to 3V3", [
        ("1", "1", "passive", "L"),
        ("2", "2", "passive", "R"),
    ]),

    ("R1", "10k", "GPIO36 pulldown - REQUIRED, no internal pull on GPIO34-39", [
        ("1", "1", "passive", "L"),
        ("2", "2", "passive", "R"),
    ]),

    ("R2", "330", "LED_Y series", [("1","1","passive","L"), ("2","2","passive","R")]),
    ("R3", "330", "LED_O series", [("1","1","passive","L"), ("2","2","passive","R")]),
    ("D1", "LED_Y", "yellow",     [("1","A","passive","L"), ("2","K","passive","R")]),
    ("D2", "LED_O", "orange",     [("1","A","passive","L"), ("2","K","passive","R")]),
]


# --- net assignment: (ref, pin_no) -> net name ------------------------------
NETS = {
    # battery -> PDB
    ("BT1","1"): "VBAT",      ("BT1","2"): "GND",
    ("J1","1"):  "VBAT",      ("J1","2"):  "GND",

    # PDB rails
    ("J1","3"):  "VBAT",      ("J1","4"):  "+12V",
    ("J1","5"):  "+5V",       ("J1","6"):  "GND",

    # fuse in the driver feed
    ("F1","1"):  "VBAT",      ("F1","2"):  "VBAT_DRV",

    # bulk cap, across the driver's own pads
    ("C1","1"):  "VBAT_DRV",  ("C1","2"):  "GND",

    # driver
    ("A1","1"):  "VBAT_DRV",  ("A1","2"):  "GND",
    ("A1","3"):  None,        # BEC 5V is an OUTPUT - deliberately unconnected
    ("A1","4"):  "DRV_TX",    ("A1","5"):  "DRV_RX",
    ("A1","6"):  "DRV_PWM",   ("A1","7"):  "GND",
    ("A1","8"):  "PHASE_A",   ("A1","9"):  "PHASE_B", ("A1","10"): "PHASE_C",

    # motor
    ("M1","1"):  "PHASE_A",   ("M1","2"):  "PHASE_B", ("M1","3"): "PHASE_C",

    # ESP32
    ("U1","1"):  "+5V",       ("U1","2"):  "GND",     ("U1","3"): "+3V3",
    ("U1","4"):  "DRV_RX",    # GPIO17 is the ESP32's TX -> driver RX
    ("U1","5"):  "DRV_TX",    # GPIO16 is the ESP32's RX <- driver TX
    ("U1","6"):  "I2C_SDA",   ("U1","7"):  "I2C_SCL",
    ("U1","8"):  "SERVO_SIG", ("U1","9"):  "BTN",
    ("U1","10"): "LED_Y",     ("U1","11"): "LED_O",
    ("U1","12"): "DRV_PWM",   # bring-up only, never together with the UART
    ("U1","13"): None,        # GPIO5/DIR has no destination: the B-G431B-ESC1
                              # input header is V- 5V TX RX PWM GND V+ with no
                              # direction pin. See NOTES.

    # IMU
    ("U2","1"):  "+3V3",      ("U2","2"):  "GND",
    ("U2","3"):  "I2C_SDA",   ("U2","4"):  "I2C_SCL",

    # servo chain
    ("U3","1"):  "+12V",      ("U3","2"):  "GND",
    ("U3","3"):  "+SERVO",    ("U3","4"):  "GND",
    ("M2","1"):  "+SERVO",    ("M2","2"):  "GND",     ("M2","3"): "SERVO_SIG",

    # button: 3V3 through the switch to the pin, 10k from the pin to GND
    ("SW1","1"): "+3V3",      ("SW1","2"): "BTN",
    ("R1","1"):  "BTN",       ("R1","2"):  "GND",

    # LEDs
    ("R2","1"):  "LED_Y",     ("R2","2"):  "LED_Y_A",
    ("D1","1"):  "LED_Y_A",   ("D1","2"):  "GND",
    ("R3","1"):  "LED_O",     ("R3","2"):  "LED_O_A",
    ("D2","1"):  "LED_O_A",   ("D2","2"):  "GND",
}

# Grid placement, in mm. Columns follow signal flow left to right.
PLACE = {
    "BT1": (40,  40),   "J1": (105, 55),   "F1": (175, 35),
    "C1":  (175, 60),   "A1": (250, 60),   "M1": (330, 50),
    "U3":  (175, 115),  "M2": (255, 115),
    "U1":  (110, 180),  "U2": (250, 160),
    "SW1": (250, 205),  "R1": (250, 222),
    "R2":  (250, 245),  "D1": (320, 245),
    "R3":  (250, 262),  "D2": (320, 262),
}

NOTES = [
    "GROUNDING - not expressible in a schematic, see the harness drawing:",
    "every GND below is one net here, but in the build each subsystem gets its",
    "own return to the PDB negative plane. Never daisy-chain a ground.",
    "",
    "A1 V- carries the motor return: 16 AWG, short. A1 GND is the signal",
    "reference for TX/RX: 26 AWG, twisted with them. They are separate pads.",
    "",
    "C1 mounts across A1's own V+ / GND pads, not at the PDB.",
    "A1 pin 3 (BEC 5V) is an OUTPUT - leave unconnected, U1 is fed from the PDB.",
    "R1 is required: GPIO34-39 have no internal pull resistor.",
    "DRV_PWM is bring-up only and must never be driven while the UART is",
    "commanding torque.",
    "",
    "GPIO5 (ESC_DIR_PIN in pins.h) has NO destination: this board's input header",
    "is V- 5V TX RX PWM GND V+ - there is no direction pin. Bidirectional drive",
    "over PWM would have to be encoded in the pulse width instead.",
]

PIN_LEN = 2.54
ROW     = 2.54


def sym_name(ref: str) -> str:
    return "".join(c for c in ref if c.isalpha()) + "_" + ref


def body_size(pins):
    left  = [p for p in pins if p[3] == "L"]
    right = [p for p in pins if p[3] == "R"]
    rows  = max(len(left), len(right), 1)
    h = (rows + 1) * ROW
    w = 33.02
    return w, h


def pin_xy(pins, idx_in_side, side, w, h):
    """Connection-point coordinate of a pin, relative to symbol origin."""
    y = h / 2 - ROW * (idx_in_side + 1)
    x = -w / 2 - PIN_LEN if side == "L" else w / 2 + PIN_LEN
    return x, y


def emit_lib_symbol(ref, value, desc, pins):
    w, h = body_size(pins)
    name = sym_name(ref)
    out = []
    out.append(f'    (symbol "cube:{name}"')
    out.append('      (pin_names (offset 1.016))')
    out.append('      (exclude_from_sim no) (in_bom yes) (on_board yes)')
    out.append(f'      (property "Reference" "{ref[0]}" (at 0 {h/2 + 2.54:.2f} 0)')
    out.append('        (effects (font (size 1.27 1.27)) (justify left)))')
    out.append(f'      (property "Value" "{value}" (at 0 {-h/2 - 2.54:.2f} 0)')
    out.append('        (effects (font (size 1.27 1.27)) (justify left)))')
    out.append(f'      (property "Footprint" "" (at 0 0 0)')
    out.append('        (effects (font (size 1.27 1.27)) hide))')
    out.append(f'      (property "Datasheet" "" (at 0 0 0)')
    out.append('        (effects (font (size 1.27 1.27)) hide))')
    out.append(f'      (property "Description" "{desc}" (at 0 0 0)')
    out.append('        (effects (font (size 1.27 1.27)) hide))')
    out.append(f'      (symbol "{name}_0_1"')
    out.append(f'        (rectangle (start {-w/2:.2f} {h/2:.2f}) (end {w/2:.2f} {-h/2:.2f})')
    out.append('          (stroke (width 0.254) (type default)) (fill (type background)))')
    out.append('      )')
    out.append(f'      (symbol "{name}_1_1"')
    li = ri = 0
    for (num, pname, ptype, side) in pins:
        if side == "L":
            x, y = pin_xy(pins, li, "L", w, h); ang = 0;   li += 1
        else:
            x, y = pin_xy(pins, ri, "R", w, h); ang = 180; ri += 1
        out.append(f'        (pin {ptype} line (at {x:.2f} {y:.2f} {ang}) (length {PIN_LEN})')
        out.append(f'          (name "{pname}" (effects (font (size 1.27 1.27))))')
        out.append(f'          (number "{num}" (effects (font (size 1.27 1.27))))')
        out.append('        )')
    out.append('      )')
    out.append('    )')
    return out


def emit_instance(ref, value, pins, x0, y0):
    w, h = body_size(pins)
    name = sym_name(ref)
    out = []
    out.append(f'  (symbol (lib_id "cube:{name}") (at {x0:.2f} {y0:.2f} 0) (unit 1)')
    out.append('    (exclude_from_sim no) (in_bom yes) (on_board yes) (dnp no)')
    out.append(f'    (uuid "{uid()}")')
    out.append(f'    (property "Reference" "{ref}" (at {x0:.2f} {y0 - h/2 - 2.54:.2f} 0)')
    out.append('      (effects (font (size 1.27 1.27)) (justify left)))')
    out.append(f'    (property "Value" "{value}" (at {x0:.2f} {y0 + h/2 + 2.54:.2f} 0)')
    out.append('      (effects (font (size 1.27 1.27)) (justify left)))')
    for (num, _pname, _pt, _s) in pins:
        out.append(f'    (pin "{num}" (uuid "{uid()}"))')
    out.append(f'    (instances (project "cube" (path "/{SHEET_UUID}"')
    out.append(f'      (reference "{ref}") (unit 1))))')
    out.append('  )')
    return out


def emit_labels(ref, pins, x0, y0):
    w, h = body_size(pins)
    out = []
    li = ri = 0
    for (num, _pname, _pt, side) in pins:
        if side == "L":
            dx, dy = pin_xy(pins, li, "L", w, h); ang, just = 180, "right"; li += 1
        else:
            dx, dy = pin_xy(pins, ri, "R", w, h); ang, just = 0, "left";    ri += 1
        if (ref, num) not in NETS:
            continue
        net = NETS[(ref, num)]
        # KiCad's Y axis runs downward on the sheet; symbol geometry is Y-up.
        x, y = x0 + dx, y0 - dy
        if net is None:
            out.append(f'  (no_connect (at {x:.2f} {y:.2f}) (uuid "{uid()}"))')
            continue
        out.append(f'  (global_label "{net}" (shape passive) (at {x:.2f} {y:.2f} {ang})')
        out.append('    (fields_autoplaced yes)')
        out.append(f'    (effects (font (size 1.27 1.27)) (justify {just})) (uuid "{uid()}"))')
    return out


SHEET_UUID = "00000000-0000-0000-0000-0000cube5ced"


def build() -> str:
    L = []
    L.append(f'(kicad_sch (version {VERSION}) (generator "cube_gen") (generator_version "8.0")')
    L.append(f'  (uuid "{SHEET_UUID}")')
    L.append(f'  (paper "{PAPER}")')
    L.append('  (title_block')
    L.append('    (title "Reaction-wheel cube - 2D prototype")')
    L.append('    (rev "A")')
    L.append('    (comment 1 "Generated by hardware/gen_schematic.py - edit the script, not this file")')
    L.append('    (comment 2 "Pin numbers for U1 follow include/ESP/pins.h")')
    L.append('  )')

    L.append('  (lib_symbols')
    for (ref, value, desc, pins) in PARTS:
        L += emit_lib_symbol(ref, value, desc, pins)
    L.append('  )')

    for (ref, value, _desc, pins) in PARTS:
        x0, y0 = PLACE[ref]
        L += emit_instance(ref, value, pins, x0, y0)
        L += emit_labels(ref, pins, x0, y0)

    y = 25.0
    for line in NOTES:
        if line:
            L.append(f'  (text "{line}" (at 40.00 {y:.2f} 0)')
            L.append(f'    (effects (font (size 1.27 1.27)) (justify left)) (uuid "{uid()}"))')
        y += 4.0

    L.append('  (sheet_instances (path "/" (page "1")))')
    L.append(')')
    return "\n".join(L) + "\n"


if __name__ == "__main__":
    import os
    text = build()
    out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "cube.kicad_sch")
    with open(out, "w") as f:
        f.write(text)
    print(f"wrote {out}  ({len(text.splitlines())} lines)")

#!/usr/bin/env python3
"""Generate hardware/cube.kicad_sch - the 2D reaction-wheel cube prototype.

Scope: the power chain and the control links. The button and status LEDs are
bring-up aids, not part of the machine, and are left out.

Symbols are defined inside the file rather than referenced from KiCad's
libraries, so the sheet opens anywhere with no library setup. Passives carry
their real glyphs; the finished modules (PDB, driver, buck, IMU, servo, ESP32)
are drawn as connectors, which is what they are - boards you wire to.

Power flows left to right on a drawn V+ bus so the topology is visible, in
particular C1 sitting across the driver's own terminals. Signals travel by
global label, which is ordinary practice and keeps the sheet readable.

    python3 hardware/gen_schematic.py
"""

VERSION    = "20231120"
PAPER      = "A3"                  # 420 x 297 mm
SHEET_UUID = "00000000-0000-0000-0000-0000cube5ced"

_uid = 0
def uid() -> str:
    """Deterministic UUIDs, so regenerating gives a clean diff."""
    global _uid
    _uid += 1
    h = f"{_uid:032x}"
    return f"{h[0:8]}-{h[8:12]}-{h[12:16]}-{h[16:20]}-{h[20:32]}"

ROW = 2.54      # pin pitch
PL  = 2.54      # pin length


# --- parts ------------------------------------------------------------------
# ref, value, glyph, [(number, name, etype, side)]
#   glyph: box | battery | fuse | cap_pol | motor
#   side:  L R T B

PARTS = [
    ("BT1", "6S LiPo", "battery", [
        ("1", "+", "power_out", "T"),
        ("2", "-", "power_out", "B"),
    ]),
    ("J1", "PDB", "box", [
        ("1", "VBAT_IN+", "power_in",  "L"),
        ("2", "VBAT_IN-", "power_in",  "L"),
        ("3", "VBAT_OUT", "power_out", "R"),
        ("4", "12V",      "power_out", "R"),
        ("5", "5V",       "power_out", "R"),
        ("6", "GND",      "power_out", "R"),
    ]),
    ("F1", "3A slow-blow", "fuse", [
        ("1", "1", "passive", "L"),
        ("2", "2", "passive", "R"),
    ]),
    ("C1", "1000uF 50V", "cap_pol", [
        ("1", "+", "passive", "T"),
        ("2", "-", "passive", "B"),
    ]),
    ("A1", "B-G431B-ESC1", "box", [
        ("1", "V+",   "power_in",  "L"),
        ("2", "V-",   "power_in",  "L"),
        ("3", "GND",  "power_in",  "L"),
        ("4", "TX",   "output",    "L"),
        ("5", "RX",   "input",     "L"),
        ("6", "PWM",  "input",     "L"),
        ("7", "5V",   "power_out", "L"),
        ("8", "OUT1", "power_out", "R"),
        ("9", "OUT2", "power_out", "R"),
        ("10","OUT3", "power_out", "R"),
    ]),
    ("M1", "GM4108H", "motor", [
        ("1", "A", "passive", "L"),
        ("2", "B", "passive", "L"),
        ("3", "C", "passive", "L"),
    ]),
    ("U3", "Servo buck 12V-6V", "box", [
        ("1", "IN+",  "power_in",  "L"),
        ("2", "IN-",  "power_in",  "L"),
        ("3", "OUT+", "power_out", "R"),
        ("4", "OUT-", "power_out", "R"),
    ]),
    ("M2", "Servo", "box", [
        ("1", "V+",  "power_in", "L"),
        ("2", "GND", "power_in", "L"),
        ("3", "SIG", "input",    "L"),
    ]),
    ("U1", "ESP32-DevKitC", "box", [
        ("1",  "5V",     "power_in",     "L"),
        ("2",  "GND",    "power_in",     "L"),
        ("3",  "3V3",    "power_out",    "L"),
        ("4",  "GPIO17", "output",       "R"),
        ("5",  "GPIO16", "input",        "R"),
        ("6",  "GPIO4",  "output",       "R"),
        ("7",  "GPIO5",  "output",       "R"),
        ("8",  "GPIO21", "bidirectional","R"),
        ("9",  "GPIO22", "bidirectional","R"),
        ("10", "GPIO13", "output",       "R"),
    ]),
    ("U2", "BNO085", "box", [
        ("1", "VIN", "power_in",     "L"),
        ("2", "GND", "power_in",     "L"),
        ("3", "SDA", "bidirectional","R"),
        ("4", "SCL", "bidirectional","R"),
    ]),
]

PLACE = {
    "BT1": (38,  62),   "J1": (84,  60),   "F1": (132, 46),
    "C1":  (158, 60),   "A1": (206, 62),   "M1": (286, 54),
    "U3":  (84, 120),   "M2": (166, 120),
    "U1":  (84, 190),   "U2": (190, 180),
}

# Power symbols: ref -> (net, at_x, at_y, kind)  kind: gnd | rail
POWER = [
    ("#PWR01", "GND",  38,  80, "gnd"),
    ("#PWR02", "GND",  158, 80, "gnd"),
    ("#PWR03", "GND",  118, 80, "gnd"),      # J1 GND
    ("#PWR04", "GND",  186, 92, "gnd"),      # A1 V- and GND
    ("#PWR05", "GND",  64,  140, "gnd"),     # U3 IN-
    ("#PWR06", "GND",  150, 134, "gnd"),     # M2 GND
    ("#PWR07", "GND",  64,  208, "gnd"),     # U1 GND
    ("#PWR08", "GND",  170, 196, "gnd"),     # U2 GND
    ("#PWR09", "+12V", 118, 40,  "rail"),    # J1 12V out
    ("#PWR10", "+12V", 64,  104, "rail"),    # U3 IN+
    ("#PWR11", "+5V",  126, 40,  "rail"),    # J1 5V out
    ("#PWR12", "+5V",  64,  174, "rail"),    # U1 5V
    ("#PWR13", "+3V3", 56,  182, "rail"),    # U1 3V3 out
    ("#PWR14", "+3V3", 170, 172, "rail"),    # U2 VIN
]

# Global labels on signal pins: (ref, pin, net)
LABELS = [
    ("A1", "4", "DRV_TX"),   ("U1", "5", "DRV_TX"),
    ("A1", "5", "DRV_RX"),   ("U1", "4", "DRV_RX"),
    ("A1", "6", "DRV_PWM"),  ("U1", "6", "DRV_PWM"),
    ("U1", "8", "I2C_SDA"),  ("U2", "3", "I2C_SDA"),
    ("U1", "9", "I2C_SCL"),  ("U2", "4", "I2C_SCL"),
    ("U1", "10", "SERVO_SIG"), ("M2", "3", "SERVO_SIG"),
    ("U3", "3", "+SERVO"),   ("M2", "1", "+SERVO"),
    ("A1", "8", "PHASE_A"),  ("M1", "1", "PHASE_A"),
    ("A1", "9", "PHASE_B"),  ("M1", "2", "PHASE_B"),
    ("A1", "10","PHASE_C"),  ("M1", "3", "PHASE_C"),
]

# Deliberate no-connects: (ref, pin, reason-for-the-note)
NOCONN = [
    ("A1", "7"),   # BEC 5V is an output
    ("U1", "7"),   # GPIO5 / DIR has no destination on this header
]


# --- geometry ---------------------------------------------------------------

def body(pins):
    L = [p for p in pins if p[3] == "L"]
    R = [p for p in pins if p[3] == "R"]
    rows = max(len(L), len(R), 1)
    return 30.48, (rows + 1) * ROW


def local_pin(pins, num, glyph):
    """Pin connection point in symbol space (Y up), relative to origin."""
    if glyph == "battery":
        return (0.0, 7.62) if num == "1" else (0.0, -7.62)
    if glyph == "cap_pol":
        return (0.0, 5.08) if num == "1" else (0.0, -5.08)
    if glyph == "fuse":
        return (-7.62, 0.0) if num == "1" else (7.62, 0.0)
    if glyph == "motor":
        idx = int(num) - 1
        return (-12.7, 2.54 - idx * ROW)
    w, h = body(pins)
    li = ri = 0
    for (n, _nm, _t, side) in pins:
        if side == "L":
            y = h / 2 - ROW * (li + 1); x = -w / 2 - PL; li += 1
        else:
            y = h / 2 - ROW * (ri + 1); x = w / 2 + PL;  ri += 1
        if n == num:
            return (x, y)
    raise KeyError(num)


def abs_pin(ref, num):
    """Pin connection point on the sheet (Y down)."""
    for (r, _v, glyph, pins) in PARTS:
        if r != ref:
            continue
        x0, y0 = PLACE[ref]
        dx, dy = local_pin(pins, num, glyph)
        return (round(x0 + dx, 2), round(y0 - dy, 2))
    raise KeyError(ref)


def pin_angle(ref, num):
    """Sheet-space direction the pin points away from its body."""
    for (r, _v, glyph, pins) in PARTS:
        if r != ref:
            continue
        if glyph in ("battery", "cap_pol"):
            return 90 if num == "1" else 270
        if glyph == "fuse":
            return 180 if num == "1" else 0
        if glyph == "motor":
            return 180
        for (n, _nm, _t, side) in pins:
            if n == num:
                return 180 if side == "L" else 0
    raise KeyError(ref)


# --- symbol graphics --------------------------------------------------------

def glyph_body(glyph, pins, name):
    s, S = [], f'        '
    def ln(pts, w=0.254, fill="none"):
        p = " ".join(f"(xy {x:.2f} {y:.2f})" for x, y in pts)
        s.append(f'{S}(polyline (pts {p}) (stroke (width {w}) (type default)) (fill (type {fill})))')

    if glyph == "battery":
        ln([(0, 7.62), (0, 2.54)])
        ln([(-3.81, 2.54), (3.81, 2.54)], 0.508)    # long plate  (+)
        ln([(-1.905, 0.635), (1.905, 0.635)])        # short plate
        ln([(-3.81, -1.27), (3.81, -1.27)], 0.508)
        ln([(-1.905, -3.175), (1.905, -3.175)])
        ln([(0, -3.175), (0, -7.62)])
        ln([(2.54, 5.08), (5.08, 5.08)])             # the + marker
        ln([(3.81, 3.81), (3.81, 6.35)])
    elif glyph == "cap_pol":
        ln([(0, 5.08), (0, 1.27)])
        ln([(-3.81, 1.27), (3.81, 1.27)], 0.508)     # positive plate
        ln([(-3.81, -0.635), (3.81, -0.635)], 0.508)
        ln([(0, -0.635), (0, -5.08)])
        ln([(-3.81, 3.175), (-1.27, 3.175)])         # the + marker
        ln([(-2.54, 1.905), (-2.54, 4.445)])
    elif glyph == "fuse":
        s.append(f'{S}(rectangle (start -7.62 -1.905) (end 7.62 1.905)'
                 f' (stroke (width 0.254) (type default)) (fill (type none)))')
        ln([(-7.62, 0), (7.62, 0)])
    elif glyph == "motor":
        s.append(f'{S}(circle (center 0 0) (radius 7.62)'
                 f' (stroke (width 0.254) (type default)) (fill (type background)))')
        for i in range(3):
            y = 2.54 - i * ROW
            ln([(-12.7, y), (-7.0, y)])
    else:
        w, h = body(pins)
        s.append(f'{S}(rectangle (start {-w/2:.2f} {h/2:.2f}) (end {w/2:.2f} {-h/2:.2f})'
                 f' (stroke (width 0.254) (type default)) (fill (type background)))')
    return s


def emit_lib_symbol(ref, value, glyph, pins):
    name = "".join(c for c in ref if c.isalpha()) + "_" + ref
    _w, h = body(pins)
    top = max(h / 2 + 2.54, 10.16)
    o = [f'    (symbol "cube:{name}"',
         '      (pin_names (offset 1.016))',
         '      (exclude_from_sim no) (in_bom yes) (on_board yes)',
         f'      (property "Reference" "{ref[0]}" (at 0 {top:.2f} 0)',
         '        (effects (font (size 1.27 1.27)) (justify left)))',
         f'      (property "Value" "{value}" (at 0 {-top:.2f} 0)',
         '        (effects (font (size 1.27 1.27)) (justify left)))',
         '      (property "Footprint" "" (at 0 0 0) (effects (font (size 1.27 1.27)) hide))',
         '      (property "Datasheet" "" (at 0 0 0) (effects (font (size 1.27 1.27)) hide))',
         f'      (symbol "{name}_0_1"']
    o += glyph_body(glyph, pins, name)
    o.append('      )')
    o.append(f'      (symbol "{name}_1_1"')
    hide = ' hide' if glyph in ("battery", "cap_pol", "fuse", "motor") else ''
    for (num, pname, ptype, _side) in pins:
        x, y = local_pin(pins, num, glyph)
        # Symbol-space pin angle: the direction from the connection point
        # toward the body, which is the opposite of how the pin faces on-sheet.
        ang = {180: 0, 0: 180, 90: 270, 270: 90}[pin_angle(ref, num)]
        o.append(f'        (pin {ptype} line (at {x:.2f} {y:.2f} {ang}) (length {PL})')
        o.append(f'          (name "{pname}" (effects (font (size 1.27 1.27)){hide}))')
        o.append(f'          (number "{num}" (effects (font (size 1.27 1.27)){hide}))')
        o.append('        )')
    o.append('      )')
    o.append('    )')
    return o


def emit_power_lib():
    o = []
    for nm, kind in (("GND", "gnd"), ("P5V", "rail"), ("P3V3", "rail"), ("P12V", "rail")):
        net = {"GND": "GND", "P5V": "+5V", "P3V3": "+3V3", "P12V": "+12V"}[nm]
        o += [f'    (symbol "cube:{nm}"',
              '      (power) (pin_names (offset 0))',
              '      (exclude_from_sim no) (in_bom no) (on_board no)',
              '      (property "Reference" "#PWR" (at 0 -6.35 0)',
              '        (effects (font (size 1.27 1.27)) hide))',
              f'      (property "Value" "{net}" (at 0 {"-3.81" if kind=="gnd" else "3.81"} 0)',
              '        (effects (font (size 1.27 1.27))))',
              ]
        if kind == "gnd":
            o += ['      (symbol "' + nm + '_0_1"',
                  '        (polyline (pts (xy -2.54 -1.27) (xy 2.54 -1.27) (xy 0 -3.81) (xy -2.54 -1.27))',
                  '          (stroke (width 0) (type default)) (fill (type outline)))',
                  '        (polyline (pts (xy 0 0) (xy 0 -1.27))',
                  '          (stroke (width 0) (type default)) (fill (type none)))',
                  '      )',
                  f'      (symbol "{nm}_1_1"',
                  f'        (pin power_in line (at 0 0 270) (length 0)',
                  f'          (name "{net}" (effects (font (size 1.27 1.27)) hide))',
                  '          (number "1" (effects (font (size 1.27 1.27)) hide))',
                  '        )',
                  '      )']
        else:
            o += ['      (symbol "' + nm + '_0_1"',
                  '        (polyline (pts (xy -2.54 1.27) (xy 2.54 1.27))',
                  '          (stroke (width 0) (type default)) (fill (type none)))',
                  '        (polyline (pts (xy 0 0) (xy 0 1.27))',
                  '          (stroke (width 0) (type default)) (fill (type none)))',
                  '      )',
                  f'      (symbol "{nm}_1_1"',
                  f'        (pin power_in line (at 0 0 90) (length 0)',
                  f'          (name "{net}" (effects (font (size 1.27 1.27)) hide))',
                  '          (number "1" (effects (font (size 1.27 1.27)) hide))',
                  '        )',
                  '      )']
        o.append('    )')
    return o


# --- sheet items ------------------------------------------------------------

OUT = []

def wire(x1, y1, x2, y2):
    OUT.append(f'  (wire (pts (xy {x1:.2f} {y1:.2f}) (xy {x2:.2f} {y2:.2f}))')
    OUT.append(f'    (stroke (width 0) (type default)) (uuid "{uid()}"))')

def junction(x, y):
    OUT.append(f'  (junction (at {x:.2f} {y:.2f}) (diameter 0) (color 0 0 0 0) (uuid "{uid()}"))')

def noconn(x, y):
    OUT.append(f'  (no_connect (at {x:.2f} {y:.2f}) (uuid "{uid()}"))')

def glabel(net, x, y, ang, just):
    OUT.append(f'  (global_label "{net}" (shape passive) (at {x:.2f} {y:.2f} {ang})')
    OUT.append('    (fields_autoplaced yes)')
    OUT.append(f'    (effects (font (size 1.27 1.27)) (justify {just})) (uuid "{uid()}"))')

def place_power(ref, net, x, y, kind):
    lib = {"GND": "GND", "+5V": "P5V", "+3V3": "P3V3", "+12V": "P12V"}[net]
    OUT.append(f'  (symbol (lib_id "cube:{lib}") (at {x:.2f} {y:.2f} 0) (unit 1)')
    OUT.append('    (exclude_from_sim no) (in_bom no) (on_board no) (dnp no)')
    OUT.append(f'    (uuid "{uid()}")')
    OUT.append(f'    (property "Reference" "{ref}" (at {x:.2f} {y - 6.35:.2f} 0)')
    OUT.append('      (effects (font (size 1.27 1.27)) hide))')
    OUT.append(f'    (property "Value" "{net}" (at {x:.2f} {y + (4.5 if net == "GND" else -4.5):.2f} 0)')
    OUT.append('      (effects (font (size 1.27 1.27))))')
    OUT.append(f'    (pin "1" (uuid "{uid()}"))')
    OUT.append(f'    (instances (project "cube" (path "/{SHEET_UUID}"')
    OUT.append(f'      (reference "{ref}") (unit 1))))')
    OUT.append('  )')

def place_part(ref, value, pins, x0, y0):
    name = "".join(c for c in ref if c.isalpha()) + "_" + ref
    _w, h = body(pins)
    top = max(h / 2 + 2.54, 10.16)
    OUT.append(f'  (symbol (lib_id "cube:{name}") (at {x0:.2f} {y0:.2f} 0) (unit 1)')
    OUT.append('    (exclude_from_sim no) (in_bom yes) (on_board yes) (dnp no)')
    OUT.append(f'    (uuid "{uid()}")')
    OUT.append(f'    (property "Reference" "{ref}" (at {x0:.2f} {y0 - top:.2f} 0)')
    OUT.append('      (effects (font (size 1.27 1.27)) (justify left)))')
    OUT.append(f'    (property "Value" "{value}" (at {x0:.2f} {y0 + top:.2f} 0)')
    OUT.append('      (effects (font (size 1.27 1.27)) (justify left)))')
    for (num, _n, _t, _s) in pins:
        OUT.append(f'    (pin "{num}" (uuid "{uid()}"))')
    OUT.append(f'    (instances (project "cube" (path "/{SHEET_UUID}"')
    OUT.append(f'      (reference "{ref}") (unit 1))))')
    OUT.append('  )')


def build() -> str:
    L = [f'(kicad_sch (version {VERSION}) (generator "cube_gen") (generator_version "8.0")',
         f'  (uuid "{SHEET_UUID}")',
         f'  (paper "{PAPER}")',
         '  (title_block',
         '    (title "Reaction-wheel cube - 2D prototype")',
         '    (rev "B")',
         '    (comment 1 "Generated by hardware/gen_schematic.py")',
         '    (comment 2 "U1 pin assignments follow include/ESP/pins.h")',
         '  )',
         '  (lib_symbols']
    for (ref, value, glyph, pins) in PARTS:
        L += emit_lib_symbol(ref, value, glyph, pins)
    L += emit_power_lib()
    L.append('  )')

    for (ref, value, _g, pins) in PARTS:
        x0, y0 = PLACE[ref]
        place_part(ref, value, pins, x0, y0)
    for (ref, net, x, y, kind) in POWER:
        place_power(ref, net, x, y, kind)

    # ---- drawn power chain -------------------------------------------------
    VBUS = 40.0                                   # the V+ rail's y
    bp, bn = abs_pin("BT1", "1"), abs_pin("BT1", "2")
    j1p, j1n = abs_pin("J1", "1"), abs_pin("J1", "2")
    j1o, j1g = abs_pin("J1", "3"), abs_pin("J1", "6")
    j1_12, j1_5 = abs_pin("J1", "4"), abs_pin("J1", "5")
    f1a, f1b = abs_pin("F1", "1"), abs_pin("F1", "2")
    c1p, c1n = abs_pin("C1", "1"), abs_pin("C1", "2")
    a1v, a1m, a1g = abs_pin("A1", "1"), abs_pin("A1", "2"), abs_pin("A1", "3")
    GNDBUS = 80.0

    # battery + -> PDB in+
    wire(bp[0], bp[1], bp[0], VBUS); wire(bp[0], VBUS, j1p[0], VBUS)
    wire(j1p[0], VBUS, j1p[0], j1p[1])
    # battery - and PDB in- down to the ground bus
    wire(bn[0], bn[1], bn[0], GNDBUS)
    wire(j1n[0], j1n[1], 64.0, j1n[1]); wire(64.0, j1n[1], 64.0, GNDBUS)
    wire(bn[0], GNDBUS, 64.0, GNDBUS)
    # PDB GND pin to the same bus
    wire(j1g[0], j1g[1], 118.0, j1g[1]); wire(118.0, j1g[1], 118.0, GNDBUS)
    wire(64.0, GNDBUS, 118.0, GNDBUS)
    junction(64.0, GNDBUS)

    # PDB VBAT_OUT -> fuse -> driver V+
    wire(j1o[0], j1o[1], f1a[0] - 6, j1o[1]); wire(f1a[0] - 6, j1o[1], f1a[0] - 6, f1a[1])
    wire(f1a[0] - 6, f1a[1], f1a[0], f1a[1])
    wire(f1b[0], f1b[1], a1v[0] - 8, f1b[1])
    wire(a1v[0] - 8, f1b[1], a1v[0] - 8, a1v[1]); wire(a1v[0] - 8, a1v[1], a1v[0], a1v[1])

    # C1 across the driver's own terminals
    wire(c1p[0], f1b[1], c1p[0], c1p[1]); junction(c1p[0], f1b[1])
    wire(c1n[0], c1n[1], c1n[0], GNDBUS)
    wire(118.0, GNDBUS, c1n[0], GNDBUS)
    junction(118.0, GNDBUS)

    # driver V- and GND to the bus
    for p in (a1m, a1g):
        wire(p[0], p[1], 186.0, p[1])
    wire(186.0, a1m[1], 186.0, 92.0)
    wire(186.0, a1g[1], 186.0, 92.0)
    junction(186.0, a1g[1])
    wire(c1n[0], GNDBUS, 186.0, GNDBUS)
    junction(186.0, GNDBUS)

    # rails off the PDB
    wire(j1_12[0], j1_12[1], 118.0, j1_12[1]); wire(118.0, j1_12[1], 118.0, 40.0)
    wire(j1_5[0], j1_5[1], 126.0, j1_5[1]);    wire(126.0, j1_5[1], 126.0, 40.0)

    # servo buck, ESP32, IMU: short stubs to their power symbols
    for ref, pin, px, py in (("U3", "1", 64.0, 104.0), ("U3", "2", 64.0, 140.0),
                             ("U1", "1", 64.0, 174.0), ("U1", "2", 64.0, 208.0),
                             ("U1", "3", 56.0, 182.0), ("U2", "1", 170.0, 172.0),
                             ("U2", "2", 170.0, 196.0), ("M2", "2", 150.0, 134.0)):
        p = abs_pin(ref, pin)
        wire(p[0], p[1], px, p[1]); wire(px, p[1], px, py)
    p = abs_pin("U3", "4")
    wire(p[0], p[1], p[0] + 8, p[1]); wire(p[0] + 8, p[1], p[0] + 8, 140.0)
    wire(p[0] + 8, 140.0, 64.0, 140.0)
    junction(64.0, 140.0)

    # ---- labels and no-connects -------------------------------------------
    for (ref, pin, net) in LABELS:
        x, y = abs_pin(ref, pin)
        ang = pin_angle(ref, pin)
        glabel(net, x, y, 180 if ang == 180 else 0, "right" if ang == 180 else "left")
    for (ref, pin) in NOCONN:
        x, y = abs_pin(ref, pin)
        noconn(x, y)

    L += OUT
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

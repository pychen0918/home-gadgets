#!/usr/bin/env python3
"""Draw the seat-timer wiring diagram as SVG (no dependencies).

Run from seat-timer/:  python3 tools/draw_wiring.py > docs/wiring.svg
Edit the WIRES table when the wiring changes, then re-run.
"""

# Header pin names as printed on the Waveshare e-Paper ESP32 Driver Board.
# "A" holds 3V3 ... 5V, "B" holds GND ... P15 (positions 1 and 7 are GND).
HEADER_A = ["3V3", "EN", "SVP", "SVN", "P34", "P35", "P32", "P33", "P25", "P26",
            "P27", "P14", "P12", "GND", "P13", "NC", "NC", "NC", "5V"]
HEADER_B = ["GND", "P23", "P22", "TX", "RX", "P21", "GND", "P19", "P18", "P5",
            "P17", "P16", "P4", "P0", "P2", "P15", "NC", "NC", "NC"]

PIN0_Y, STEP = 200, 24
BX0, BX1 = 420, 600          # header B on the left edge, header A on the right edge

COLOR = {"5v": "#d62828", "gnd": "#222222", "3v3": "#f77f00",
         "led": "#2a9d8f", "tx": "#1d70b8", "rx": "#7b2cbf", "buz": "#c9a227"}


def pin_y(i):
    return PIN0_Y + i * STEP


# (colour, label, polyline points). Each route starts at a module pin.
WIRES = [
    # LED bar input end (S|V|G) -> header A
    ("led", "LED S -> P33", [(860, 42), (890, 42), (890, pin_y(7)), (BX1, pin_y(7))]),
    ("5v", "LED V -> 5V", [(860, 55), (940, 55), (940, pin_y(18)), (BX1, pin_y(18))]),
    ("gnd", "LED G -> GND", [(860, 68), (915, 68), (915, pin_y(13)), (BX1, pin_y(13))]),
    # LED bar output end (DOUT|V|G) -> radar power
    ("5v", "LED out V -> radar VCC", [(240, 55), (215, 55), (215, 392), (200, 392)]),
    ("gnd", "LED out G -> radar GND", [(240, 68), (228, 68), (228, 416), (200, 416)]),
    # Radar UART -> header B
    ("rx", "radar RX -> P17", [(200, 440), (BX0, pin_y(10))]),
    ("tx", "radar TX -> P16", [(200, 464), (BX0, pin_y(11))]),
    # Buzzer
    ("gnd", "buzzer GND -> GND", [(460, 150), (460, 170), (395, 170), (395, pin_y(0)), (BX0, pin_y(0))]),
    ("3v3", "buzzer VCC -> 3V3", [(510, 150), (510, 185), (620, 185), (620, pin_y(0)), (BX1, pin_y(0))]),
    ("buz", "buzzer I/O -> P32", [(560, 150), (560, 165), (640, 165), (640, pin_y(6)), (BX1, pin_y(6))]),
]

USED_A = {0, 6, 7, 13, 18}
USED_B = {0, 10, 11}

out = []
w = out.append
w('<svg xmlns="http://www.w3.org/2000/svg" width="1000" height="820" viewBox="0 0 1000 820" '
  'font-family="Helvetica, Arial, sans-serif" font-size="13">')
w('<rect width="1000" height="820" fill="#ffffff"/>')


def box(x, y, bw, bh, fill, title, sub=None):
    w(f'<rect x="{x}" y="{y}" width="{bw}" height="{bh}" rx="6" fill="{fill}" stroke="#333" stroke-width="1.5"/>')
    w(f'<text x="{x + bw / 2}" y="{y + (bh / 2 if not sub else bh / 2 - 6)}" text-anchor="middle" '
      f'dominant-baseline="middle" font-weight="bold">{title}</text>')
    if sub:
        w(f'<text x="{x + bw / 2}" y="{y + bh / 2 + 10}" text-anchor="middle" dominant-baseline="middle" '
          f'font-size="11" fill="#444">{sub}</text>')


def pin(x, y, label, anchor, dx):
    w(f'<circle cx="{x}" cy="{y}" r="4" fill="#fff" stroke="#333" stroke-width="1.5"/>')
    w(f'<text x="{x + dx}" y="{y}" text-anchor="{anchor}" dominant-baseline="middle" font-size="12">{label}</text>')


# ESP32 driver board
w(f'<rect x="{BX0}" y="180" width="{BX1 - BX0}" height="{pin_y(18) + 20 - 180}" rx="8" fill="#1f4e79" stroke="#123" stroke-width="2"/>')
w(f'<text x="{(BX0 + BX1) / 2}" y="{pin_y(9) - 10}" text-anchor="middle" fill="#fff" font-weight="bold">ESP32</text>')
w(f'<text x="{(BX0 + BX1) / 2}" y="{pin_y(9) + 10}" text-anchor="middle" fill="#cfe" font-size="11">e-Paper Driver Board</text>')
for i, name in enumerate(HEADER_B):
    used = i in USED_B
    w(f'<circle cx="{BX0}" cy="{pin_y(i)}" r="5" fill="{"#ffd166" if used else "#bbb"}" stroke="#000"/>')
    w(f'<text x="{BX0 + 10}" y="{pin_y(i)}" dominant-baseline="middle" fill="{"#fff" if used else "#9ab"}" '
      f'font-size="12" font-weight="{"bold" if used else "normal"}">{name}</text>')
for i, name in enumerate(HEADER_A):
    used = i in USED_A
    w(f'<circle cx="{BX1}" cy="{pin_y(i)}" r="5" fill="{"#ffd166" if used else "#bbb"}" stroke="#000"/>')
    w(f'<text x="{BX1 - 10}" y="{pin_y(i)}" text-anchor="end" dominant-baseline="middle" '
      f'fill="{"#fff" if used else "#9ab"}" font-size="12" font-weight="{"bold" if used else "normal"}">{name}</text>')
w(f'<text x="{BX0}" y="{pin_y(18) + 36}" font-size="11" fill="#555">header B</text>')
w(f'<text x="{BX1}" y="{pin_y(18) + 36}" text-anchor="end" font-size="11" fill="#555">header A</text>')

# LED bar (input end on the right, output end on the left)
box(240, 30, 620, 50, "#e9f5f2", "WS2812B 10-LED bar", "data flows S -> DOUT; V and G pass straight through")
for y, s in ((42, "S"), (55, "V"), (68, "G")):
    pin(860, y, s, "end", -8)
for y, s in ((42, "DOUT"), (55, "V"), (68, "G")):
    pin(240, y, s, "start", 8)
w('<line x1="240" y1="42" x2="200" y2="42" stroke="#999" stroke-width="2" stroke-dasharray="4 3"/>')
w('<text x="196" y="42" text-anchor="end" dominant-baseline="middle" font-size="11" fill="#777">not connected</text>')

# Radar
box(60, 370, 140, 140, "#f3eefc", "LD2410C", "radar")
for y, s in ((392, "VCC"), (416, "GND"), (440, "RX"), (464, "TX"), (488, "OUT")):
    pin(200, y, s, "end", -8)
w('<text x="130" y="530" text-anchor="middle" font-size="11" fill="#777">OUT not connected</text>')

# Buzzer
box(430, 84, 160, 66, "#fff6dc", "Buzzer", "active, low-level trigger")
for x, s in ((460, "GND"), (510, "VCC"), (560, "I/O")):
    w(f'<text x="{x}" y="142" text-anchor="middle" font-size="10" fill="#333">{s}</text>')
    w(f'<circle cx="{x}" cy="150" r="4" fill="#fff" stroke="#333" stroke-width="1.5"/>')

# E-paper on the FPC connector
w(f'<rect x="{BX0 - 110}" y="{pin_y(18) + 60}" width="{BX1 - BX0 + 220}" height="70" rx="4" fill="#f4f4f4" stroke="#333" stroke-width="1.5"/>')
w(f'<text x="{(BX0 + BX1) / 2}" y="{pin_y(18) + 82}" text-anchor="middle" font-weight="bold">2.66" e-paper</text>')
w(f'<text x="{(BX0 + BX1) / 2}" y="{pin_y(18) + 100}" text-anchor="middle" font-size="11" fill="#444">'
  f'colour panel: FPC connector on the board</text>')
w(f'<text x="{(BX0 + BX1) / 2}" y="{pin_y(18) + 116}" text-anchor="middle" font-size="11" fill="#444">'
  f'mono module: SCK 13, MOSI 14, CS 15, DC 27, RST 26, BUSY 25</text>')
w(f'<line x1="{(BX0 + BX1) / 2}" y1="{pin_y(18) + 20}" x2="{(BX0 + BX1) / 2}" y2="{pin_y(18) + 60}" '
  'stroke="#777" stroke-width="10" stroke-opacity="0.4"/>')

# Wires: white halo underneath so crossings stay readable
for kind, _, pts in WIRES:
    p = " ".join(f"{x},{y}" for x, y in pts)
    w(f'<polyline points="{p}" fill="none" stroke="#fff" stroke-width="7" stroke-linejoin="round"/>')
    w(f'<polyline points="{p}" fill="none" stroke="{COLOR[kind]}" stroke-width="3" stroke-linejoin="round"/>')

# Legend
legend = [("5v", "5V"), ("gnd", "GND"), ("3v3", "3.3V"), ("led", "LED data (P33)"),
          ("tx", "radar TX -> P16"), ("rx", "radar RX -> P17"), ("buz", "buzzer I/O (P32)")]
lx, ly = 60, 590
w(f'<text x="{lx}" y="{ly - 18}" font-weight="bold">Wires</text>')
for i, (k, s) in enumerate(legend):
    y = ly + i * 20
    w(f'<line x1="{lx}" y1="{y}" x2="{lx + 30}" y2="{y}" stroke="{COLOR[k]}" stroke-width="4"/>')
    w(f'<text x="{lx + 40}" y="{y}" dominant-baseline="middle">{s}</text>')
w(f'<text x="20" y="790" font-size="11" fill="#555">All connections are female Dupont / PH2.0 cables onto header pins; '
  'no breadboard, no soldering. Follow the labels printed on each module, not wire colours.</text>')
w('</svg>')
print("\n".join(out))

"""Draw the wiring diagrams for wiring.md with schemdraw.

Usage:
    pip install schemdraw            # SVG output needs nothing else
    python make_diagrams.py [OUT_DIR] [--png]   # --png needs matplotlib

Writes one SVG (or PNG) per circuit into OUT_DIR (default: this folder).
"""
import os
import sys

import schemdraw
import schemdraw.elements as elm

args = [a for a in sys.argv[1:] if not a.startswith("--")]
PNG = "--png" in sys.argv
OUT = args[0] if args else os.path.dirname(os.path.abspath(__file__))
schemdraw.use("matplotlib" if PNG else "svg")

# Colors per signal type so wires are easy to follow
C5V = "#d62728"    # 5V
C3V3 = "#ff7f0e"   # 3V3
CGND = "#222222"   # GND
CSIG = "#1f77b4"   # signal
CESP = "#2ca02c"   # ESP pin


def save(d, name):
    path = os.path.join(OUT, name + (".png" if PNG else ".svg"))
    d.save(path, dpi=150) if PNG else d.save(path)
    print("wrote", path)


def esp_pin(text):
    return elm.Dot(open=True).color(CESP).label(text, loc="left", color=CESP)


# --- 1. Overview: which D1 mini pin goes where ------------------------------
def overview():
    with schemdraw.Drawing(show=False) as d:
        d.config(fontsize=12, unit=2.5)
        right = [
            ("D1", "GPIO5"), ("D2", "GPIO4"), ("D5", "GPIO14"),
            ("D6", "GPIO12"), ("D7", "GPIO13"), ("D8", "GPIO15"),
        ]
        left = [("5V", ""), ("3V3", ""), ("GND", "")]
        pins = [elm.IcPin(name=n, pin=g, side="right", anchorname=n) for n, g in right]
        pins += [elm.IcPin(name=n, side="left", anchorname=n) for n, _ in left]
        ic = elm.Ic(pins=pins, edgepadW=4, pinspacing=1.0, leadlen=0.6).label("D1 mini\n(ESP8266)", "center", fontsize=14)

        targets = {
            "D1": ("DHT11 out", CSIG, "fig. 5"),
            "D2": ("CN1 TX", CSIG, "fig. 2"),
            "D5": ("CN1 RX", CSIG, "fig. 2"),
            "D6": ("CN3 IR", CSIG, "fig. 3"),
            "D7": ("CN3 IR", CSIG, "fig. 4"),
            "D8": ("BUZ1", CSIG, "fig. 6"),
        }
        for name, (what, col, fig) in targets.items():
            elm.Line().at(getattr(ic, name)).right(4).color(col)
            elm.Dot(open=True).color(col).label(f"{what}  ({fig})", loc="right", color=col, fontsize=11)

        power = {
            "5V": ("CN1 +5V", C5V),
            "3V3": ("DHT11 +", C3V3),
            "GND": ("CN1 GND, DHT11 −", CGND),
        }
        for name, (what, col) in power.items():
            elm.Line().at(getattr(ic, name)).left(3).color(col)
            elm.Dot(open=True).color(col).label(what, loc="left", color=col, fontsize=11)
        save(d, "01_overview")


# --- 2. CN1 voltage dividers -------------------------------------------------
def dividers():
    with schemdraw.Drawing(show=False) as d:
        d.config(fontsize=12, unit=2.5)
        for i, (src, pin) in enumerate([("CN1 TX", "D2 / GPIO4"), ("CN1 RX", "D5 / GPIO14")]):
            start = (0, -i * 5.5)
            elm.Dot(open=True).at(start).color(C5V).label(src, loc="left", color=C5V)
            elm.Resistor().right().label("330Ω").color(CSIG)
            j = elm.Dot().color(CSIG)
            elm.Line().right(2).color(CESP)
            elm.Dot(open=True).color(CESP).label(pin, loc="right", color=CESP)
            elm.Resistor().at(j.center).down().label("680Ω", loc="bottom").color(CSIG)
            elm.Ground().color(CGND)
        save(d, "02_cn1_dividers")


# --- 3. IR sniff diode -------------------------------------------------------
def ir_sniff():
    with schemdraw.Drawing(show=False) as d:
        d.config(fontsize=12, unit=2.5)
        elm.Dot(open=True).color(CESP).label("D6 / GPIO12", loc="left", color=CESP)
        elm.Diode().right().label("1N4148", loc="top").color(CSIG)
        elm.Line().right(1.5).color(CSIG)
        elm.Dot(open=True).color(CSIG).label("CN3 IR", loc="right", color=CSIG)
        save(d, "03_ir_sniff")


def npn_driver(d, pin_label, collector_to):
    """2N2222 low-side switch: pin -> 1k -> base, 10k base-GND, emitter GND."""
    q = elm.BjtNpn(circle=True).label("2N2222", loc="right", ofst=(0.2, 0)).color(CSIG)
    elm.Line().at(q.base).left(1).color(CSIG)
    j = elm.Dot().color(CSIG)
    elm.Resistor().left().label("1k").color(CESP)
    elm.Dot(open=True).color(CESP).label(pin_label, loc="left", color=CESP)
    elm.Resistor().at(j.center).down().label("10k", loc="bottom").color(CSIG)
    elm.Ground().color(CGND)
    elm.Line().at(q.emitter).down(1.25).color(CGND)
    elm.Ground().color(CGND)
    return q


# --- 4. IR inject -----------------------------------------------------------
def ir_inject():
    with schemdraw.Drawing(show=False) as d:
        d.config(fontsize=12, unit=2.5)
        q = npn_driver(d, "D7 / GPIO13", "CN3 IR")
        elm.Line().at(q.collector).up(1.5).color(CSIG)
        elm.Dot(open=True).color(CSIG).label("CN3 IR", loc="right", color=CSIG)
        save(d, "04_ir_inject")


# --- 5. DHT11 ---------------------------------------------------------------
def dht11():
    with schemdraw.Drawing(show=False) as d:
        d.config(fontsize=12, unit=2.5)
        pins = [elm.IcPin(name="+", side="left", anchorname="vcc"),
                elm.IcPin(name="out", side="left", anchorname="out"),
                elm.IcPin(name="−", side="left", anchorname="gnd")]
        ic = elm.Ic(pins=pins, pinspacing=1.2, edgepadW=1.5, leadlen=0.6).label("DHT11\nmodule", "center")
        for anchor, text, col in [("vcc", "3V3", C3V3), ("out", "D1 / GPIO5", CESP), ("gnd", "GND", CGND)]:
            elm.Line().at(getattr(ic, anchor)).left(3).color(col)
            elm.Dot(open=True).color(col).label(text, loc="left", color=col)
        save(d, "05_dht11")


# --- 6. Buzzer --------------------------------------------------------------
def buzzer():
    with schemdraw.Drawing(show=False) as d:
        d.config(fontsize=12, unit=2.5)
        q = npn_driver(d, "D8 / GPIO15", "BUZ1")
        elm.Line().at(q.collector).up(0.75).color(CSIG)
        bot = elm.Dot().color(CSIG)
        # piezo branch and 1k discharge branch in parallel
        elm.Line().left(1.5).color(CSIG)
        elm.Crystal().up().label("BUZ1\npiezo", loc="top").color(CSIG)
        elm.Line().right(1.5).color(C5V)
        elm.Line().at(bot.center).right(1.5).color(CSIG)
        elm.Resistor().up().label("1k", loc="bottom").color(CSIG)
        elm.Line().left(1.5).color(C5V)
        top = elm.Dot().color(C5V)
        elm.Line().up(1).color(C5V)
        elm.Dot(open=True).color(C5V).label("5V", loc="right", color=C5V)
        save(d, "06_buzzer")


# --- 7. Power / decoupling ----------------------------------------------------
def power():
    with schemdraw.Drawing(show=False) as d:
        d.config(fontsize=12, unit=2.5)
        elm.Dot(open=True).color(C5V).label("CN1 +5V", loc="left", color=C5V)
        elm.Line().right(2).color(C5V)
        a = elm.Dot().color(C5V)
        elm.Line().right(3).color(C5V)
        b = elm.Dot().color(C5V)
        elm.Line().right(2).color(C5V)
        elm.Dot(open=True).color(C5V).label("D1 mini 5V", loc="right", color=C5V)
        elm.Capacitor(polar=True).at(a.center).down().label("220–470µF\n≥10V", loc="top").color(C5V)
        elm.Ground().color(CGND)
        elm.Capacitor().at(b.center).down().label("100nF", loc="bottom").color(C5V)
        elm.Ground().color(CGND)
        save(d, "07_power")


# --- 0. Full schematic: everything on one sheet --------------------------------
def full():
    with schemdraw.Drawing(show=False) as d:
        d.config(fontsize=11, unit=2)
        sp = 4.5  # vertical distance between the signal lanes on the right
        # schemdraw lists IC pins bottom to top
        right = [("D8", "GPIO15"), ("D7", "GPIO13"), ("D6", "GPIO12"), ("D5", "GPIO14"), ("D2", "GPIO4")]
        left = [("5V", ""), ("GND", ""), ("3V3", ""), ("D1", "GPIO5")]
        pins = [elm.IcPin(name=n, pin=g, side="right", anchorname=n) for n, g in right]
        pins += [elm.IcPin(name=n, pin=g, side="left", anchorname=n) for n, g in left]
        ic = elm.Ic(pins=pins, pinspacing=sp, edgepadW=3, leadlen=0.6).label(
            "D1 mini\n(ESP8266)", "center", fontsize=14)

        def y(name):
            return getattr(ic, name)[1]

        x_conn = ic.D2[0] + 19  # x of the AC connector pins CN1 / CN3

        def ac_pin(text):
            elm.Dot(open=True).color("black").label(text, loc="right", fontsize=11)

        # D2 / D5: 330/680 dividers to CN1 TX / RX
        for pin, text in (("D2", "CN1 TX"), ("D5", "CN1 RX")):
            elm.Line().at(getattr(ic, pin)).right(1.5).color(CESP)
            j = elm.Dot().color(CSIG)
            elm.Resistor().right().label("330Ω").color(CSIG)
            elm.Line().tox(x_conn).color(CSIG)
            ac_pin(text)
            elm.Resistor().at(j.center).down().label("680Ω", loc="bottom").color(CSIG)
            elm.Ground().color(CGND)

        # D6: IR sniff diode to CN3 IR
        elm.Line().at(ic.D6).right(1.5).color(CESP)
        elm.Diode().right().label("1N4148", loc="top").color(CSIG)
        elm.Line().tox(x_conn).color(CSIG)
        ac_pin("CN3 IR")

        def npn(pin, x_base):
            """1k to base, 10k base-GND, emitter GND. Returns the transistor."""
            elm.Line().at(getattr(ic, pin)).tox(x_base - 5).color(CESP)
            elm.Resistor().right().label("1k").color(CESP)
            j = elm.Dot().color(CSIG)
            elm.Line().tox(x_base).color(CSIG)
            q = elm.BjtNpn(circle=True).anchor("base").label("2N2222", loc="right", ofst=(0.1, 0)).color(CSIG)
            elm.Resistor().at(j.center).down().label("10k", loc="bottom").color(CSIG)
            elm.Ground().color(CGND)
            elm.Line().at(q.emitter).down(1).color(CGND)
            elm.Ground().color(CGND)
            return q

        # D7: IR inject, collector joins the CN3 IR line
        q7 = npn("D7", ic.D7[0] + 8)
        elm.Line().at(q7.collector).toy(y("D6")).color(CSIG)
        elm.Dot().color(CSIG)

        # D8: buzzer driver, piezo || 1k from collector to 5V
        q8 = npn("D8", ic.D8[0] + 14)
        elm.Line().at(q8.collector).up(0.5).color(CSIG)
        bot = elm.Dot().color(CSIG)
        elm.Line().left(1.2).color(CSIG)
        elm.Crystal().up(2.2).label("BUZ1\npiezo", loc="top").color(CSIG)
        elm.Line().right(1.2).color(C5V)
        elm.Line().at(bot.center).right(1.2).color(CSIG)
        elm.Resistor().up(2.2).label("1k", loc="bottom").color(CSIG)
        elm.Line().left(1.2).color(C5V)
        elm.Dot().color(C5V)
        elm.Vdd().label("5V").color(C5V)

        # Left side: power from CN1 with buffer caps
        elm.Line().at(ic["5V"]).left(2).color(C5V)
        p = elm.Dot().color(C5V)
        elm.Line().left(3).color(C5V)
        a = elm.Dot().color(C5V)
        elm.Line().left(3).color(C5V)
        elm.Dot(open=True).color("black").label("CN1 +5V", loc="left", fontsize=11)
        elm.Capacitor(polar=True).at(p.center).down().label("470µF\n≥10V", loc="bottom").color(C5V)
        elm.Ground().color(CGND)
        elm.Capacitor().at(a.center).down().label("100nF", loc="bottom").color(C5V)
        elm.Ground().color(CGND)

        elm.Line().at(ic.GND).left(8).color(CGND)
        g = elm.Dot().color(CGND)
        elm.Dot(open=True).color("black").label("CN1 GND", loc="left", fontsize=11)
        elm.Line().at(g.center).down(0.8).color(CGND)
        elm.Ground().color(CGND)

        # Left side: DHT11 module pins
        x_left = ic.D1[0] - 8
        elm.Line().at(ic.D1).tox(x_left).color(CESP)
        elm.Dot(open=True).color("black").label("DHT11 out", loc="left", fontsize=11)
        elm.Line().at(ic["3V3"]).tox(x_left).color(C3V3)
        elm.Dot(open=True).color("black").label("DHT11 +", loc="left", fontsize=11)
        elm.Dot(open=True).at((x_left, y("3V3") - 1.8)).color("black").label("DHT11 −", loc="left", fontsize=11)
        elm.Line().right(1.5).color(CGND)
        elm.Ground().color(CGND)
        save(d, "00_full")


if __name__ == "__main__":
    os.makedirs(OUT, exist_ok=True)
    for f in (full, overview,dividers, ir_sniff, ir_inject, dht11, buzzer, power):
        f()

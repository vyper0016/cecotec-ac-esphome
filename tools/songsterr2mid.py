"""Songsterr part JSONs -> type-1 MIDI (one track per part, track index = part index + 1).

usage: python songsterr2mid.py out.mid part0.json part1.json ...
Pitch = tuning[string] + fret. Handles signatures, tempo automation, repeats, ties, dots, tuplets.
Prints a measure -> beat map so recipes can use --start in beats.
"""
import json
import sys
from fractions import Fraction as F

import mido

TPB = 480


def beat_len(b):
    n, d = b["duration"]  # dots and tuplets are already folded in ([3, 8] = dotted quarter)
    return F(4 * n, d)  # quarter-note beats


def unroll(measures):
    """Measure indices in play order (simple repeats, no alternate endings)."""
    order, start, i, done = [], 0, 0, {}
    while i < len(measures):
        m = measures[i]
        if m.get("repeatStart"):
            start = i
        order.append(i)
        rep = m.get("repeat")
        if rep and done.get(i, 0) < rep - 1:
            done[i] = done.get(i, 0) + 1
            i = start
            continue
        i += 1
    return order


def part_events(d, order):
    ev, t, sig = [], F(0), [4, 4]
    sigs = []
    for mi in range(len(d["measures"])):
        sig = d["measures"][mi].get("signature", sig)
        sigs.append(F(4 * sig[0], sig[1]))
    starts = []
    for mi in order:
        m = d["measures"][mi]
        starts.append((mi, t))
        for v in m["voices"]:
            vt = t
            for b in v["beats"]:
                L = beat_len(b)
                if not b.get("rest"):
                    for n in b["notes"]:
                        if n.get("rest") or n.get("dead") or "fret" not in n:
                            continue
                        p = d["tuning"][n["string"]] + n["fret"]
                        if n.get("tie"):  # extend last note on this pitch
                            for e in reversed(ev):
                                if e[2] == p:
                                    e[1] = vt + L - e[0]
                                    break
                            continue
                        ev.append([vt, L, p])
                vt += L
        t += sigs[mi]
    return ev, starts


def main():
    out, files = sys.argv[1], sys.argv[2:]
    parts = [json.load(open(f, encoding="utf8")) for f in files]
    order = unroll(parts[0]["measures"])
    mid = mido.MidiFile(type=1, ticks_per_beat=TPB)
    meta = mido.MidiTrack()
    mid.tracks.append(meta)
    starts = None
    for ch, d in enumerate(parts):
        ev, st = part_events(d, order)
        starts = starts or st
        if ch == 0:
            tempos = []
            for a in d.get("automations", {}).get("tempo", []):
                for mi, t in st:
                    if mi == a["measure"]:
                        tempos.append((t + F(a.get("position", 0)), a["bpm"]))
                        break
            last = 0
            for t, bpm in sorted(set(tempos)):
                tick = int(t * TPB)
                meta.append(mido.MetaMessage("set_tempo", tempo=mido.bpm2tempo(bpm), time=tick - last))
                last = tick
        msgs = []
        for t, L, p in ev:
            msgs.append((int(t * TPB), 1, p))
            msgs.append((int((t + L) * TPB), 0, p))
        msgs.sort()
        tr = mido.MidiTrack()
        tr.append(mido.MetaMessage("track_name", name=d.get("name", "")))
        last = 0
        ch_ = ch if ch < 9 else ch + 1
        for tick, on, p in msgs:
            typ = "note_on" if on else "note_off"
            tr.append(mido.Message(typ, note=p, velocity=100 if on else 0, channel=ch_ % 16, time=tick - last))
            last = tick
        mid.tracks.append(tr)
    mid.save(out)
    print("measure -> beat:", " ".join(f"{mi}:{float(t):g}" for mi, t in starts))


main()

"""MIDI -> RTTTL for the ESP's piezo beeper (ESPHome `rtttl`).

Needs: pip install mido

Workflow for a new melody:
  1. Find a MIDI (vgmusic.com for games, bitmidi.com, mutopiaproject.org for classical).
  2. Inspect it: which track has the melody, tempo, where the phrase starts (in beats):
       python midi2rtttl.py song.mid [--start BEAT]
  3. Convert a window of that track:
       python midi2rtttl.py song.mid NAME --tracks 1 --start 0 --beats 8 --shift 12
  4. Add the output line as a MEL_ string to MELODIES in cecotec/melodies.h
     (and the option name into the "Test melody" select in cecotec-ac.yaml, same position;
     both lists are alphabetical, so check the MELODY_ indices and MELODY_COUNT assert after).

How it converts:
  - Skyline: at each onset the highest note of the selected tracks wins (piezo plays one note).
  - Onsets/lengths are quantized to --grid beats (quarter = 1 beat).
  - Each note gets ONE RTTTL length (1,2,4,8,16,32,64, dotted) closest to how long it sounds,
    never past the next onset; the rest of the gap becomes rests. No ties in RTTTL.
    64 is fine per note in ESPHome, only the d= default is limited to 1..32.
  - Tempo comes from the MIDI (set_tempo at --start), so timing matches the original.
    RTTTL has one tempo: a warning lists tempo changes inside the window.

Options:
  --shift SEMIS  transpose. ESPHome rtttl only plays octaves 4-7 (C4 = 262 Hz). The piezo is
                 loudest around 2.7 kHz (E7/F7), so aim for octaves 6-7.
  --scale X      stretch beats and bpm by X (real time unchanged). Use it when notes don't fit
                 RTTTL lengths: x1.875 turns 0.267-beat triplets into 8ths.
  --grid G       quantize step in (scaled) beats, default 0.25 (16th); 0.0625 for 64ths.
                 Onsets round half up, lengths half down (a tie keeps notes detached),
                 the final note rounds up.
  --bpm X        override the MIDI tempo.
  --triplets     also use triplet lengths: ESPHome takes any per-note number, 6 = quarter triplet,
                 12 = 8th triplet, 24 = 16th triplet. Use with a grid of 1/12 or 1/24.
  --len-grid G   quantize note lengths to G instead of --grid (staccato 16ths on a fine grid).
  --legato X     a note sounding >= X of the gap to the next onset fills the gap (default 0.75).
  --min-pitch N  ignore notes below MIDI number N (drops bass/accompaniment from the skyline).
  --defaults D,O force d= and o=; with --pad (rests up to the window end) parts made separately
                 join by concatenating their bodies after the second ':'.

Recipes used for the current melodies (2026-11-01). Source MIDIs are saved in tools/midi/
(run from tools/:  python midi2rtttl.py midi/smb.mid smb --tracks 1,2 ...):
  local file        original source                                   options
  smb.mid           vgmusic nes/Smbtheme.mid                          smb --tracks 1,2 --start 0 --beats 4 --shift 12
  smb1up.mid        vgmusic nes/smb-1up.mid                           oneup --tracks 1 --start 0 --beats 5
  ffvictory.mid     vgmusic nes/Ffvictory.mid                         ffvictory --tracks 1 --start 1.64 --beats 6.35 --scale 1.875 --grid 0.5 --shift 12
  tetris.mid        vgmusic gameboy/Tet_Music_A-KM.mid                tetris --tracks 3 --start 0 --beats 16 --shift 12
  megalovania.mid   vgmusic windows/Undertale_-_Megalovania_v1_2.mid  megalovania --tracks 3 --start 0 --beats 8 --shift 36
  allstar.mid       bitmidi uploads/94634.mid                         allstar --tracks 1 --start 3 --beats 8.4 --shift 36
  imperial.mid      bitmidi uploads/29414.mid (format 0)              imperial --tracks 100 --start 20 --beats 16 --shift 24
  elise.mid         bitmidi uploads/28362.mid                         elise --tracks 1 --start 0 --beats 5.5 --shift 12
  toccata.mid       mutopia BachJS/BWV565/ToccataFugue/ToccataFugue.mid  toccata --tracks 1,2 --start 0 --beats 9 --grid 0.0625 --shift 24 --bpm 30
               --bpm 30 = half the MIDI's mechanical 60, which sounded too fast. Then by hand
               (written at b=60 with doubled lengths): mordents (64a,64g,8a.,16p.) on each A,
               "2d6" resolution after the chord, shorter rests and final chord
  goldenwind.mid    github.com/Xanderev/JOJO---Giorno-Theme           goldenwind --start 48.5 --beats 8 --shift 24 --min-pitch 60 --bpm 135
               --bpm 135 = the recording's tempo (the MIDI runs at 120)
  stardust.mid      songsterr 434611 (Yugo Kanno, Main Theme), all parts  stardust --tracks 4 --start 120 --beats 24 --shift 24
               part JSONs -> MIDI with songsterr2mid.py; track 4 = Saxes 1 (the theme), 1 = lead guitar
  forelle.mid       mutopia SchubertF/D550/forelle/forelle.mid         forelle --tracks 3 --start 11.5 --beats 15.5 --shift 20 --bpm 100
               track 3 = voice; key (A major) and tempo from a Samsung washer recording
  Extended versions:
    smblong   midi/smb.mid --tracks 1,2 --start 0 --beats 52 --shift 12 --grid 0.08333333 --len-grid 0.25 --triplets --legato 0.6
    gwlong    midi/goldenwind.mid --start 48.5 --beats 79 --shift 24 --min-pitch 60 --bpm 135, then by hand
              8a# -> 8a in the theme (the piano arrangement's A#; the recording plays A natural)
    megalong  two parts, both --shift 12 --defaults 16,6: --tracks 1,5 --start 32 --beats 48 --pad,
              then --tracks 4 --start 80 --beats 16 (track 4 has the D' A G F C' A F E line)
    fflong    two parts, both --tracks 1 --scale 1.25 --grid 0.16666667 --triplets --defaults 4,6:
              --start 1.64 --beats 6.4 --shift 12 --pad, then --start 8.04 --beats 51.2 --shift 24
    sclong    three parts, all --defaults 8,6: --tracks 1 --start 68 --beats 16 --shift 24 --pad,
              --tracks 4 --start 84 --beats 32 --shift 36 --pad, then --tracks 4 --start 116 --beats 28
              --shift 24 with its first note 1f4 -> 1f5 by hand (same opening as the first statement)
    toclong   body of MEL_TOCCATA, "4p", then --tracks 1,2 --start 11.75 --beats 33.25 --grid 0.04166667
              --triplets --shift 24 --defaults 32,5 --bpm 60 (Prestissimo runs at MIDI tempo)
    forellelong  midi/forelle.mid --tracks 3 --start 11.5 --beats 39.5 --shift 20 --bpm 100 (whole verse)
  (vgmusic = https://www.vgmusic.com/music/console/nintendo/... or /computer/microsoft/...)
"""
import argparse
import math
import sys

import mido

NOTE_NAMES = ["c", "c#", "d", "d#", "e", "f", "f#", "g", "g#", "a", "a#", "b"]
DEFAULT_BPM = 120  # MIDI default before the first set_tempo
EPS = 1e-6
# (beats, RTTTL denominator, dot), longest first. 64 works per note in ESPHome, not as d=.
DURS = []
for den in (1, 2, 4, 8, 16, 32, 64):
    b = 4 / den
    DURS.append((b * 1.5, f"{den}", "."))
    DURS.append((b, f"{den}", ""))
DURS.sort(key=lambda x: -x[0])
# --triplets: ESPHome divides the whole note by any per-note number, so 6 = quarter triplet,
# 12 = 8th triplet, 24 = 16th triplet. Exact, no --scale trick needed.
TRIPLET_DURS = sorted(DURS + [(4 / den, f"{den}", "") for den in (3, 6, 12, 24, 48)], key=lambda x: -x[0])


def load_notes(path):
    """Returns ([(track_id, name, channels, [(start, dur, pitch)])], [(beat, bpm)])."""
    mid = mido.MidiFile(path)
    tpb = mid.ticks_per_beat
    tracks = []
    tempos = []
    for ti, tr in enumerate(mid.tracks):
        t = 0
        on = {}
        notes = []
        name = ""
        chans = set()
        for msg in tr:
            t += msg.time
            if msg.type == "track_name":
                name = msg.name
            if msg.type == "set_tempo":
                tempos.append((t / tpb, mido.tempo2bpm(msg.tempo)))
            if msg.type == "note_on" and msg.velocity > 0:
                if msg.channel == 9:
                    continue  # drums
                chans.add(msg.channel)
                # retriggered before its note-off: queue it, the first note-off ends the oldest
                on.setdefault((msg.channel, msg.note), []).append(t)
            elif msg.type in ("note_off", "note_on"):
                k = (msg.channel, msg.note)
                if on.get(k):
                    s = on[k].pop(0)
                    notes.append((s / tpb, (t - s) / tpb, msg.note, msg.channel))
        tracks.append((ti, name, sorted(chans), sorted(notes)))
    if mid.type == 0:
        # single-track file: split by channel as pseudo tracks 100 + channel
        ti, name, chans, notes = tracks[0]
        tracks = [(100 + c, f"{name} ch{c}", [c], [n for n in notes if n[3] == c]) for c in chans]
    tracks = [(ti, name, ch, [n[:3] for n in ns]) for ti, name, ch, ns in tracks]
    return tracks, sorted(tempos)


def inspect(path, start):
    tracks, tempos = load_notes(path)
    print("tempos (beat, bpm):", [(round(b, 2), round(bpm, 1)) for b, bpm in tempos[:6]])
    for ti, name, chans, notes in tracks:
        if not notes:
            continue
        lo = min(n[2] for n in notes)
        hi = max(n[2] for n in notes)
        print(f"track {ti:3} {name[:30]!r:32} ch{chans} n={len(notes):4} pitch {lo}-{hi} first@{notes[0][0]:.2f}")
        head = [n for n in notes if n[0] >= start][:24]
        print("    ", " ".join(f"{NOTE_NAMES[p % 12]}{p // 12 - 1}@{s:.2f}/{d:.2f}" for s, d, p in head))


def fit(beats, durs):
    """Largest representable duration <= beats (None if below 1/64)."""
    for b, den, dot in durs:
        if b <= beats + EPS:
            return b, den, dot
    return None


def rests(beats, durs):
    out = []
    while (f := fit(beats, durs)) is not None:
        b, den, dot = f
        out.append((den, dot, "p", None))
        beats -= b
    if beats > EPS:
        print(f"WARNING: {beats:.4f} beat gap below 1/64 dropped, timing drifts. Use a coarser --grid",
              file=sys.stderr)
    return out


def token(den, dot, name, octave, ddef, odef):
    d = "" if den == ddef else den
    o = "" if octave is None or octave == odef else str(octave)
    return f"{d}{name}{o}{dot}"


def convert(a):
    tracks, tempos = load_notes(a.file)
    sel = [int(x) for x in a.tracks.split(",")] if a.tracks else [ti for ti, *_ in tracks]
    notes = [n for ti, _, _, ns in tracks if ti in sel for n in ns if a.min_pitch <= n[2] <= a.max_pitch]
    end = a.start + a.beats
    durs = TRIPLET_DURS if a.triplets else DURS

    # round() is half-to-even: 0.375 and 0.625 on a 0.25 grid would both land on 0.5.
    def q_on(x):  # onsets: half up
        return math.floor(x / a.grid + 0.5 + EPS) * a.grid

    len_grid = a.len_grid or a.grid

    def q_dur(x):  # lengths: half down, a tie keeps the note detached
        return math.ceil(x / len_grid - 0.5 - EPS) * len_grid

    # skyline: highest pitch per quantized onset; notes cut at the window end
    by_on = {}
    for s, d, p in notes:
        if s < a.start - EPS or s >= end - EPS:
            continue
        qs = q_on((s - a.start) * a.scale)
        d = min(d, end - s) * a.scale
        if qs not in by_on or p > by_on[qs][1]:
            ring = math.floor(d / len_grid + 0.5 + EPS) * len_grid  # half up
            by_on[qs] = (max(q_dur(d), len_grid), p, max(ring, len_grid))
    onsets = sorted(by_on)
    if not onsets:
        sys.exit("no notes in that window")
    bpm = a.bpm or next((bpm for b, bpm in reversed(tempos) if b <= a.start + EPS), DEFAULT_BPM)
    if not a.bpm:
        changes = sorted({round(t, 1) for b, t in tempos if a.start + EPS < b < end - EPS} - {round(bpm, 1)})
        if changes:
            print(f"WARNING: tempo changes inside the window ({changes} bpm), RTTTL has one tempo: "
                  f"using {bpm:.1f}. Shorten the window or set --bpm", file=sys.stderr)
    bpm *= a.scale

    toks = []
    if onsets[0] > 1e-6:
        toks += rests(onsets[0], durs)
    for i, on in enumerate(onsets):
        dur, p, ring = by_on[on]
        last = i + 1 == len(onsets)
        if last:
            dur = ring  # nothing follows: a tie rounds up and lets the final note ring
        ioi = dur if last else onsets[i + 1] - on
        if last and a.pad:
            ioi = (a.beats * a.scale) - on  # fill to the window end: the next part starts on time
        sound = min(dur, ioi)
        if sound >= a.legato * ioi:
            sound = ioi  # legato
        cands = [x for x in durs if x[0] <= ioi + EPS] or [durs[-1]]
        b, den, dot = min(cands, key=lambda x: abs(x[0] - sound))
        p += a.shift
        toks.append((den, dot, NOTE_NAMES[p % 12], p // 12 - 1))
        if not last or a.pad:
            toks += rests(ioi - b, durs)

    octs = [o for *_, o in toks if o is not None]
    if min(octs) < 4 or max(octs) > 7:
        print(f"WARNING: octaves {min(octs)}-{max(octs)}, ESPHome plays 4-7 only. Adjust --shift", file=sys.stderr)
    # defaults giving the shortest string; d= accepts 1,2,4,8,16,32 only. Sorted, so ties are
    # deterministic. --defaults fixes them, so parts made separately can be joined into one melody.
    if a.defaults:
        ddef, odef = a.defaults.split(",")
        dens, octs_def = [ddef], [int(odef)]
    else:
        dens = sorted({d for d, *_ in toks if d in ("1", "2", "4", "8", "16", "32")} or {"32"}, key=int)
        octs_def = sorted(set(octs))
    body, ddef, odef = min(
        ((",".join(token(d, dot, n, o, dd, oo) for d, dot, n, o in toks), dd, oo)
         for dd in dens for oo in octs_def),
        key=lambda x: len(x[0]),
    )
    total = sum(4 / int(d) * (1.5 if dot else 1) for d, dot, *_ in toks) * 60 / bpm
    print(f"  ({len(body)} chars)", file=sys.stderr)
    print(f"{a.name}:d={ddef},o={odef},b={round(bpm)}:{body}")
    print(f"  ({len(toks)} tokens, {total:.1f} s)", file=sys.stderr)


if __name__ == "__main__":
    ap = argparse.ArgumentParser(description="MIDI -> RTTTL (see module docstring)")
    ap.add_argument("file")
    ap.add_argument("name", nargs="?", help="melody name; omit to inspect the file")
    ap.add_argument("--tracks", default="", help="comma list of track ids from inspect")
    ap.add_argument("--start", type=float, default=0, help="first beat (quarter = 1)")
    ap.add_argument("--beats", type=float, default=16, help="window length in beats")
    ap.add_argument("--shift", type=int, default=0, help="transpose in semitones")
    ap.add_argument("--grid", type=float, default=0.25, help="quantize step in beats")
    ap.add_argument("--bpm", type=float, default=0, help="override tempo")
    ap.add_argument("--scale", type=float, default=1, help="stretch beats and bpm")
    ap.add_argument("--len-grid", type=float, default=0,
                    help="quantize note lengths to this instead of --grid (fewer odd lengths)")
    ap.add_argument("--triplets", action="store_true", help="allow triplet lengths (3,6,12,24,48)")
    ap.add_argument("--legato", type=float, default=0.75,
                    help="a note sounding >= this share of the gap to the next one fills it")
    ap.add_argument("--min-pitch", type=int, default=0, help="ignore notes below (MIDI number)")
    ap.add_argument("--max-pitch", type=int, default=127, help="ignore notes above (MIDI number)")
    ap.add_argument("--defaults", default="", help="force d= and o=, e.g. 16,6 (for joining parts)")
    ap.add_argument("--pad", action="store_true", help="end with rests up to the window end (for joining parts)")
    args = ap.parse_args()
    if args.name:
        convert(args)
    else:
        inspect(args.file, args.start)

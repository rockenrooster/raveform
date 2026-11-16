"""RAVEFORM bonus Tracks (design/modes/music/trackNN.json): rendered, measured against the set list, checked note by note.

The bonus Tracks are outside the set list (design/modes/README.md): Mirrorball is the home of TRACERS, Toybox of JAM.
They are not embedded by the build until the modes are wired, so they live beside the modes' pages, not in design/music.
This is setlist.py's check for them: each is rendered with the prototype renderer, measured like the ten, flagged where it
leaves the range of the three Tracks first accepted by ear (First Light, Solar Wind, Reef), and every note of its pads,
stabs, arps and bass lines is checked against its scale. For Toybox it also checks the rule its page is built on: nothing
above the bass but C D E G A.

Writes design/modes/music/previews/trackNN_cycle.mp3 (the staged cycle of each) and bonus_sampler.mp3 (six bars of each:
the end of the Build and the start of the Drop). WAV files go to captures/work/bonus (not kept in the repository).

Usage:  . .\\scripts\\env.ps1; python -I design\\tools\\bonus_tracks.py [--no-audio]
"""
import glob
import json
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import musicproto as mp  # noqa: E402
import setlist as sl  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
BONUS = os.path.join(ROOT, "design", "modes", "music")
WORK = os.path.join(ROOT, "captures", "work", "bonus")
PITCH = {"C": 0, "D": 2, "E": 4, "F": 5, "G": 7, "A": 9, "B": 11}
NAMES = ["C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"]


def tonic(key):
    name = key.split()[0]
    pc = PITCH[name[0]]
    if len(name) > 1 and name[1] == "b":
        pc -= 1
    if len(name) > 1 and name[1] == "#":
        pc += 1
    return pc % 12


def check_notes(track):
    """Every note of every pad, stab, arp and bass line against the Track's scale; returns the notes outside it."""
    t = tonic(track["key"])
    allowed = {(t + s) % 12 for s in track["scale"]}
    bad = []
    for name, chord in track["chords"].items():
        for part in ("pad", "stab", "arp"):
            for n in chord[part]:
                if n % 12 not in allowed:
                    bad.append("%s %s %s" % (name, part, NAMES[n % 12]))
        for pattern, notes in track["bass"].items():
            if pattern == "format":
                continue
            for step, semis, _vel, _length in notes:
                n = chord["bass"] + int(semis)
                if n % 12 not in allowed:
                    bad.append("%s bass %s step %s: %s" % (name, pattern, step, NAMES[n % 12]))
        if chord["bass"] % 12 not in allowed:
            bad.append("%s bass root %s" % (name, NAMES[chord["bass"] % 12]))
    return bad


def check_five(track, five):
    """Toybox's rule: the lead, the stabs and the pads use only the five notes given (pitch classes)."""
    bad = []
    for name, chord in track["chords"].items():
        for part in ("pad", "stab", "arp"):
            for n in chord[part]:
                if n % 12 not in five and not (part == "pad" and n == min(chord["pad"])):
                    bad.append("%s %s %s" % (name, part, NAMES[n % 12]))
    return bad


def assist_line(chord):
    """The notes the auto-aim plays: every second step, the off-beat's (GAME_DESIGN 2)."""
    return [NAMES[n % 12] + str(n // 12 - 1) for n in chord["arp"][1::2]]


def main():
    audio = "--no-audio" not in sys.argv
    os.makedirs(WORK, exist_ok=True)
    os.makedirs(os.path.join(BONUS, "previews"), exist_ok=True)
    ref = []
    for n in sl.REFERENCE:
        track = json.load(open(os.path.join(sl.MUSIC, "track%02d.json" % n), encoding="utf-8"))
        master, stems, _info = mp.render(track)
        ref.append(sl.measure(master, stems))
        print("reference track%02d %s rendered" % (n, track["name"]), flush=True)
    lo = {k: min(r[k] for r in ref) for k in ref[0]}
    hi = {k: max(r[k] for r in ref) for k in ref[0]}

    rows, clips = [], []
    for path in sorted(glob.glob(os.path.join(BONUS, "track[0-9][0-9].json"))):
        track = json.load(open(path, encoding="utf-8"))
        n = int(track["id"][-2:])
        master, stems, _info = mp.render(track)
        m = sl.measure(master, stems)
        flags = []
        if m["himid"] > hi["himid"] + 2.0 or m["high"] > hi["high"] + 2.0:
            flags.append("bright")
        if m["low"] < lo["low"] - 3.0:
            flags.append("thin")
        if m["low"] > hi["low"] + 2.0:
            flags.append("boomy")
        if m["phone"] < lo["phone"] - 1.5:
            flags.append("weak on a phone speaker")
        if m["lead_hz"] > hi["lead_hz"] * 1.25:
            flags.append("lead bright")
        if m["peak"] > -1.0:
            flags.append("peak")
        outside = check_notes(track)
        rows.append((n, track, m, flags, outside))
        bar = 240.0 / track["bpm"]
        a, b = int(sl.SAMPLER_BARS[0] * bar * mp.SR), int(sl.SAMPLER_BARS[1] * bar * mp.SR)
        clips.append((n, track["name"], master[a:b].copy()))
        if audio:
            wav = os.path.join(WORK, "track%02d_cycle.wav" % n)
            mp.write_wav(wav, master)
            sl.mp3(wav, os.path.join(BONUS, "previews", "track%02d_cycle.mp3" % n))
        print("rendered track%02d %s" % (n, track["name"]), flush=True)

    print()
    print("accepted range (Tracks 1-3):  low %.1f..%.1f  himid %.1f..%.1f  high %.1f..%.1f  phone %.1f..%.1f  lead %.0f..%.0f Hz" % (
        lo["low"], hi["low"], lo["himid"], hi["himid"], lo["high"], hi["high"], lo["phone"], hi["phone"], lo["lead_hz"], hi["lead_hz"]))
    print("%-3s %-12s %4s %-9s %6s %6s %6s %6s %6s %6s %6s %6s %8s  %s" % (
        "", "Track", "BPM", "key", "LUFS", "peak", "crest", "low", "mid", "himid", "high", "phone", "lead Hz", "outside the accepted range"))
    for n, track, m, flags, outside in rows:
        print("%-3d %-12s %4d %-9s %6.1f %6.1f %6.1f %6.1f %6.1f %6.1f %6.1f %6.1f %8.0f  %s" % (
            n, track["name"], track["bpm"], track["key"], m["lufs"], m["peak"], m["crest"], m["low"], m["mid"], m["himid"], m["high"],
            m["phone"], m["lead_hz"], ", ".join(flags) or "-"))
    print()
    for n, track, _m, _flags, outside in rows:
        print("track%02d %s: notes outside its scale: %s" % (n, track["name"], ", ".join(outside) or "none"))
        for name in track["progression"]:
            print("   %-6s the auto-aim's line: %s" % (name, " ".join(assist_line(track["chords"][name]))))
        if track["name"] == "Toybox":
            five = {0, 2, 4, 7, 9}
            bad = check_five(track, five)
            print("   Toybox's rule (nothing above the bass but C D E G A): %s" % (", ".join(bad) or "holds"))

    if audio and clips:
        fade, gap = int(0.25 * mp.SR), int(0.35 * mp.SR)
        parts, t, marks = [], 0.0, []
        for n, name, clip in clips:
            ramp = np.linspace(0.0, 1.0, fade)[:, None]
            clip[:fade] *= ramp
            clip[-fade:] *= ramp[::-1]
            marks.append((t, n, name))
            parts += [clip, np.zeros((gap, 2))]
            t += (len(clip) + gap) / mp.SR
        wav = os.path.join(WORK, "bonus_sampler.wav")
        mp.write_wav(wav, np.concatenate(parts))
        sl.mp3(wav, os.path.join(BONUS, "previews", "bonus_sampler.mp3"))
        print()
        print("bonus_sampler.mp3 (%.0f s): " % t + "  ".join("%d:%02d %d %s" % (int(s) // 60, int(s) % 60, n, name) for s, n, name in marks))


if __name__ == "__main__":
    main()

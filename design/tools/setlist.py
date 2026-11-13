"""RAVEFORM set list: every Track rendered, measured and cut into one sampler to listen to.

The composer cannot hear. This renders each design/music/trackNN.json with the prototype renderer,
prints the numbers that stand in for ears (loudness, peak, the share of the mix in each band, what a
phone speaker keeps, how bright the lead is), flags a Track that falls outside the range of the three
Tracks already accepted by ear (First Light, Solar Wind, Reef), and writes:

  design/music/previews/trackNN_cycle.mp3   the staged cycle of each Track
  design/music/previews/setlist_sampler.mp3 six bars of every Track in order: the end of the Build and
                                            the start of the Drop

Usage:  . .\\scripts\\env.ps1; python -I design\\tools\\setlist.py [4 5 6 ...]      (no numbers = all)
WAV files go to captures/work/setlist (not kept in the repository).
"""
import glob
import json
import math
import os
import subprocess
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import musicproto as mp  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
MUSIC = os.path.join(ROOT, "design", "music")
WORK = os.path.join(ROOT, "captures", "work", "setlist")
REFERENCE = (1, 2, 3)                      # the yardstick: accepted by ear first (all ten were, on 2026-11-12)
SAMPLER_BARS = (10, 16)                    # bars 11-16 of the staged cycle: two of Build, four of Drop


def mp3(wav, out):
    subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", wav, "-codec:a", "libmp3lame", "-qscale:a", "2", out], check=True)


def measure(master, stems):
    rms = float(np.sqrt(np.mean(master ** 2)))
    peak = float(np.max(np.abs(master)))
    bands = dict(mp.band_report(master))
    phone = np.stack([mp.fft_filter(master[:, ch], mp.bp(280.0, 9000.0, 3)) for ch in range(2)], axis=1)
    lead = stems.get("lead")
    centroid = 0.0
    if lead is not None:
        mono = lead.mean(axis=1)
        spec = np.abs(np.fft.rfft(mono)) ** 2
        f = np.fft.rfftfreq(len(mono), 1.0 / mp.SR)
        centroid = float((spec * f).sum() / max(spec.sum(), 1e-20))
    return dict(lufs=mp.lufs(master), peak=20 * math.log10(peak), crest=20 * math.log10(peak / rms),
                low=10 * math.log10(10 ** (bands["sub 30-80"] / 10) + 10 ** (bands["bass 80-250"] / 10)),
                mid=10 * math.log10(10 ** (bands["lowmid 250-800"] / 10) + 10 ** (bands["mid 800-2.5k"] / 10)),
                himid=bands["himid 2.5k-6k"], high=bands["high 6k-16k"],
                phone=20 * math.log10(float(np.sqrt(np.mean(phone ** 2))) / rms), lead_hz=centroid)


def main():
    os.makedirs(WORK, exist_ok=True)
    wanted = [int(a) for a in sys.argv[1:]]
    files = sorted(glob.glob(os.path.join(MUSIC, "track[0-9][0-9].json")))
    rows, clips = [], []
    for path in files:
        track = json.load(open(path, encoding="utf-8"))
        n = int(track["id"][-2:])
        master, stems, info = mp.render(track)
        m = measure(master, stems)
        rows.append((n, track["name"], track["bpm"], track["key"], m))
        bar = 240.0 / track["bpm"]
        a, b = int(SAMPLER_BARS[0] * bar * mp.SR), int(SAMPLER_BARS[1] * bar * mp.SR)
        clips.append((n, track["name"], master[a:b].copy()))
        if not wanted or n in wanted:
            wav = os.path.join(WORK, "track%02d_cycle.wav" % n)
            mp.write_wav(wav, master)
            mp3(wav, os.path.join(MUSIC, "previews", "track%02d_cycle.mp3" % n))
        print("rendered track%02d %s" % (n, track["name"]), flush=True)

    ref = [m for n, _, _, _, m in rows if n in REFERENCE]
    lo = {k: min(r[k] for r in ref) for k in ref[0]}
    hi = {k: max(r[k] for r in ref) for k in ref[0]}
    print()
    print("%-3s %-12s %4s %-11s %6s %6s %6s %6s %6s %6s %6s %6s %8s  %s" % (
        "", "Track", "BPM", "key", "LUFS", "peak", "crest", "low", "mid", "himid", "high", "phone", "lead Hz", "outside the accepted range"))
    for n, name, bpm, key, m in rows:
        flags = []
        if n not in REFERENCE:
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
        print("%-3d %-12s %4d %-11s %6.1f %6.1f %6.1f %6.1f %6.1f %6.1f %6.1f %6.1f %8.0f  %s" % (
            n, name, bpm, key, m["lufs"], m["peak"], m["crest"], m["low"], m["mid"], m["himid"], m["high"], m["phone"], m["lead_hz"], ", ".join(flags) or "-"))

    # the sampler: each clip faded in and out over a quarter of a second, with a short gap
    fade, gap = int(0.25 * mp.SR), int(0.35 * mp.SR)
    parts, t, marks = [], 0.0, []
    for n, name, clip in clips:
        ramp = np.linspace(0.0, 1.0, fade)[:, None]
        clip[:fade] *= ramp
        clip[-fade:] *= ramp[::-1]
        marks.append((t, n, name))
        parts += [clip, np.zeros((gap, 2))]
        t += (len(clip) + gap) / mp.SR
    wav = os.path.join(WORK, "setlist_sampler.wav")
    mp.write_wav(wav, np.concatenate(parts))
    mp3(wav, os.path.join(MUSIC, "previews", "setlist_sampler.mp3"))
    print()
    print("setlist_sampler.mp3 (%.0f s): " % t + "  ".join("%d:%02d %d %s" % (int(s) // 60, int(s) % 60, n, name) for s, n, name in marks))


if __name__ == "__main__":
    main()

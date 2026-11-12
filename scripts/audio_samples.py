"""Compares the engine's baked one-shots (raveform_headless --dump-samples <dir>) with the prototype's
own synthesis (design/tools/musicproto.py, sfxproto.py), one sample at a time: length, RMS (both are
peak-normalised by their recipes), the energy in the six bands of musicproto.band_report and the
spectral centre. Noise-based sounds differ sample by sample (another random generator) but must agree
in these statistics.

  python -I -B scripts/audio_samples.py captures/m2/samples
"""
import copy
import json
import math
import os
import sys

import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "design", "tools"))
import musicproto as mp  # noqa: E402
import sfxproto as sp  # noqa: E402

EDGES = [30, 80, 250, 800, 2500, 6000, 16000]


def load(name):
    with open(os.path.join(ROOT, "design", "music", name), encoding="utf-8") as f:
        return json.load(f)


def resolved_patch(track, variant):
    patch = copy.deepcopy(mp._SOUND["patch"])
    for source in (variant or {}, track):
        for name, value in source.get("patch", {}).items():
            if isinstance(value, dict):
                patch[name].update(value)
            else:
                patch[name] = value
    return patch


def stats(x):
    x = np.asarray(x, dtype=np.float64)
    mono = x.mean(axis=1) if x.ndim == 2 else x
    spec = np.abs(np.fft.rfft(mono)) ** 2
    f = np.fft.rfftfreq(len(mono), 1.0 / mp.SR)
    total = max(1e-30, float(spec[(f >= 30) & (f < 16000)].sum()))
    bands = [10 * math.log10(max(1e-30, float(spec[(f >= EDGES[i]) & (f < EDGES[i + 1])].sum())) / total) for i in range(6)]
    centre = float((f * spec).sum() / max(1e-30, spec.sum()))
    rms = 20 * math.log10(max(1e-12, float(np.sqrt(np.mean(x ** 2)))))
    return len(mono), rms, bands, centre


def main():
    d = sys.argv[1]
    tracks = [load("track01.json"), load("track02.json")]
    variants = load("variants.json")["variants"]
    refs = {}
    for t, tr in enumerate(tracks):
        for v, var in enumerate(variants):
            p = resolved_patch(tr, var)
            saved = copy.deepcopy(mp.PATCH)
            mp.PATCH.clear()
            mp.PATCH.update(p)
            refs["kick_%d_%d" % (t, v)] = mp.make_kick(p["kick_hz"])
            refs["hat_%d_%d" % (t, v)] = mp.make_hat(p["hat"]["closed_decay"], 21)
            refs["ohat_%d_%d" % (t, v)] = mp.make_hat(p["hat"]["open_decay"], 22)
            mp.PATCH.clear()
            mp.PATCH.update(saved)
        bar = 240.0 / tr["bpm"]
        refs["swellhalf_%d" % t] = mp.make_swell(bar * 0.5)
        refs["swellbar_%d" % t] = mp.make_swell(bar)
        for bars in (4, 8):
            refs["riser_%d_%d" % (t, bars)] = mp.make_riser(bars * bar)
    refs["clap"] = mp.make_clap()
    refs["snare"] = mp.make_snare()
    refs["shaker"] = mp.make_shaker()
    refs["pulse"] = mp.make_pulse()
    refs["impact"] = mp.make_impact()
    for name in os.listdir(d):
        if not name.endswith(".f32"):
            continue
        base = name[:-4]
        kind, _, note = base.rpartition("_")
        if kind in ("kill_small", "kill_medium", "kill_large", "shard", "spawn") and note.isdigit():
            fn = {"kill_small": sp.kill_small, "kill_medium": sp.kill_medium, "kill_large": sp.kill_large,
                  "shard": sp.shard, "spawn": sp.spawn}[kind]
            refs[base] = fn(mp.midi_hz(int(note)))
    singles = {"boost": sp.boost, "upgrade": sp.upgrade, "dash": sp.dash, "perfect": sp.perfect_dash, "hit": sp.hit,
               "blast": sp.nova_blast, "tick": sp.tick, "enemy_shot": sp.enemy_shot, "elite": sp.elite_spawn,
               "ui_tick": sp.ui_tick, "game_over": sp.game_over,
               "ui_confirm": lambda: sp.two_note(1318.5, 1760.0, 0.04, 0.09),
               "ui_back": lambda: sp.two_note(1760.0, 1318.5, 0.04, 0.09),
               "shield": lambda: sp.two_note(659.3, 880.0, 0.07, 0.16, soft=True)}
    for k, fn in singles.items():
        refs[k] = fn()
    for t, tr in enumerate(tracks):
        step32 = 240.0 / tr["bpm"] / 32.0
        for s in (1, 4, 8):
            refs["inhale_%d_%d" % (t, s)] = sp.nova_inhale(s * step32)

    print("%-18s %7s %7s %7s  %s  %s" % ("sample", "frames", "rms", "d rms", "band diff (sub bass lowmid mid himid high)", "centre"))
    worst = 0.0
    for base in sorted(refs):
        path = os.path.join(d, base + ".f32")
        if not os.path.exists(path):
            continue
        ref = refs[base]
        eng = np.fromfile(path, dtype="<f4").astype(np.float64)
        if ref.ndim == 2:
            eng = eng.reshape(-1, 2)
        ne, re_, be, ce = stats(eng)
        nr, rr, br, cr = stats(ref)
        diffs = [a - b for a, b in zip(be, br)]
        # Only the bands that hold at least 1% of the sound's energy count toward the verdict.
        significant = [abs(dv) for dv, b in zip(diffs, br) if b > -20.0]
        w = max(significant) if significant else 0.0
        worst = max(worst, w)
        print("%-18s %7d %7.2f %+7.2f  %s  %6.0f/%6.0f Hz  %s" % (
            base, ne, re_, re_ - rr, " ".join("%+5.1f" % x for x in diffs), ce, cr, "" if ne == nr else "(ref %d frames)" % nr))
    print("worst difference in a band holding >= 1%% of a sound's energy: %.2f dB" % worst)


if __name__ == "__main__":
    main()

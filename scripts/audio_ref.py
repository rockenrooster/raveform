"""Reference renders from the music prototype (design/tools/musicproto.py), for the M2 engine match.

Runs the prototype as a library (nothing under design/ is written: run with `python -I -B`) and saves,
for every Track x sound variant, the prototype's master as a float32 WAV plus its numbers as JSON:
integrated loudness, true peak, the six bands of musicproto.band_report, the per-section levels, the
normalisation gain the prototype applied, and the per-stem levels.

  python -I -B scripts/audio_ref.py captures/m2/ref            (all Tracks x variants)
  python -I -B scripts/audio_ref.py captures/m2/ref track01 club
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

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import audio_metrics as am  # noqa: E402

MUSIC = os.path.join(ROOT, "design", "music")
BONUS = os.path.join(ROOT, "design", "modes", "music")  # M8 part 4: the bonus Tracks, where the lead keeps them


def load(name):
    path = os.path.join(MUSIC, name)
    if not os.path.exists(path):
        path = os.path.join(BONUS, name)
    with open(path, encoding="utf-8") as f:
        return json.load(f)


def render(track_id, variant_id, out_dir):
    track = load(track_id + ".json")
    variants = {v["id"]: v for v in load("variants.json")["variants"]}
    variant = variants[variant_id]
    # The prototype normalises its master to target_lufs inside render(): record the loudness it
    # measured before that gain (its only lufs() call), so the gain it applied is known too.
    seen = []
    real_lufs = mp.lufs

    def spy(x):
        v = real_lufs(x)
        seen.append(v)
        return v

    mp.lufs = spy
    try:
        master, stems, info = mp.render(track, variant)
    finally:
        mp.lufs = real_lufs
    name = "%s_%s" % (track_id, variant_id)
    wav = os.path.join(out_dir, name + ".ref.wav")
    am.write_wav_float(wav, master)
    m = am.measure(master, track["bpm"], info["sections"])
    m["stems_rms_db"] = {k: 20 * math.log10(max(1e-9, float(np.sqrt(np.mean(v ** 2))))) for k, v in sorted(stems.items())}
    m["stem_bands_db"] = am.stem_bands(stems)
    m["seconds"] = info["seconds"]
    m["bars"] = info["bars"]
    if seen:
        m["pre_gain_lufs"] = seen[0]
        m["applied_gain_db"] = mp.TARGET_LUFS - seen[0]  # uncapped; capped only if the peak hit 0.891
    with open(os.path.join(out_dir, name + ".ref.json"), "w", encoding="utf-8") as f:
        json.dump(m, f, indent=1)
    print("%-16s lufs %6.2f  tp %6.2f  peak %6.2f  bands %s" % (
        name, m["lufs"], m["true_peak_db"], m["peak_db"], " ".join("%.1f" % b for b in m["bands_db"])))


def main():
    out_dir = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, "captures", "m2", "ref")
    os.makedirs(out_dir, exist_ok=True)
    # M5: every Track of the set list (design/music/trackNN.json).
    every = sorted(os.path.splitext(f)[0] for d in (MUSIC, BONUS) for f in os.listdir(d) if f.startswith("track") and f.endswith(".json"))
    tracks = [sys.argv[2]] if len(sys.argv) > 2 else every
    variants = [sys.argv[3]] if len(sys.argv) > 3 else ["club", "arcade", "velvet"]
    for t in tracks:
        for v in variants:
            render(t, v, out_dir)


if __name__ == "__main__":
    main()

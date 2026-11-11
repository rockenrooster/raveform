"""RAVEFORM sound variants: the same twelve bars of a Track in each variant of design/music/variants.json.

For the M2 listening checkpoint. Each variant plays four bars of full Groove with the lead, a four-bar
Build and four bars of Drop, at the same loudness, one after another with a short silence between, so
they can be compared by ear in one file. The numbers printed are the guard rails for a composer who
cannot hear: none of the variants may lose the phone-speaker band, pile up in the bass or turn harsh.

Usage:  python design/tools/musicvariants.py [track.json] [out.wav]
        -> design/music/previews/sound_variants.wav (convert to mp3 with ffmpeg for sharing)
"""
import copy
import json
import math
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import musicproto as mp  # noqa: E402

MUSIC = os.path.dirname(mp.SOUND_FILE)
GAP = 0.9   # seconds of silence between variants

EXCERPT = [
    {"section": "groove", "bars": 4, "layers": {"kick": 1, "hat_off": 1, "bass": "groove", "lead": 1, "clap": 1, "stab": 1, "pulse": 1, "shaker": 1, "pad": 0.6}},
    {"section": "build", "bars": 4, "layers": {"kick": 1, "hat_off": 1, "bass": "groove", "lead": 1, "pad": 0.8, "stab": 1}},
    {"section": "drop", "bars": 4, "layers": {"kick": 1, "clap": 1, "hat_16": 1, "ohat": 1, "bass": "drop", "lead": "double", "stab": 1, "pulse": 1, "shaker": 1, "pad": 1.0}},
]


def centroid(x):
    """Spectral centre of gravity in Hz: a single number for how bright something is."""
    mono = x.mean(axis=1)
    spec = np.abs(np.fft.rfft(mono)) ** 2
    f = np.fft.rfftfreq(len(mono), 1.0 / mp.SR)
    keep = (f >= 30) & (f < 16000)
    return float((f[keep] * spec[keep]).sum() / max(spec[keep].sum(), 1e-30))


def measure(x, stems):
    peak = float(np.max(np.abs(x)))
    rms = float(np.sqrt(np.mean(x ** 2)))
    phone = np.stack([mp.fft_filter(x[:, ch], mp.bp(280.0, 9000.0, 3)) for ch in range(2)], axis=1)
    return dict(lufs=mp.lufs(x), peak=20 * math.log10(peak), crest=20 * math.log10(peak / rms),
                phone=20 * math.log10(float(np.sqrt(np.mean(phone ** 2))) / rms), bands=[v for _, v in mp.band_report(x)],
                bright=centroid(x), lead=centroid(stems["lead"]), bass=centroid(stems["bass"]),
                tail=20 * math.log10(max(1e-9, float(np.sqrt(np.mean(stems["reverb"] ** 2)))) / rms))


def main():
    args = [a for a in sys.argv[1:]]
    src = args[0] if args else os.path.join(MUSIC, "track01.json")
    out = args[1] if len(args) > 1 else os.path.join(MUSIC, "previews", "sound_variants.wav")
    track = json.load(open(src, encoding="utf-8"))
    track = copy.deepcopy(track)
    track["arrangement"] = EXCERPT
    data = json.load(open(os.path.join(MUSIC, "variants.json"), encoding="utf-8"))
    parts, rows, t = [], [], 0.0
    for v in data["variants"]:
        master, stems, info = mp.render(track, v)
        rows.append((v, t, measure(master, stems)))
        parts += [master, np.zeros((int(GAP * mp.SR), 2))]
        t += len(master) / mp.SR + GAP
    mp.write_wav(out, np.concatenate(parts[:-1]))
    ref = rows[0][2]
    print("%-8s %6s %6s %6s %6s %6s   %s" % ("variant", "start", "LUFS", "peak", "crest", "phone", "sub   bass  lowmid  mid  himid  high   (dB of total; in brackets: change from the reference)"))
    for v, start, m in rows:
        bands = "  ".join("%5.1f" % b for b in m["bands"])
        delta = "  ".join("%+4.1f" % (b - r) for b, r in zip(m["bands"], ref["bands"]))
        print("%-8s %5.1fs %6.1f %6.2f %6.1f %6.1f   %s   [%s]" % (v["id"], start, m["lufs"], m["peak"], m["crest"], m["phone"], bands, delta))
    print("brightness (spectral centre, Hz) and reverb tail (dB below the mix):")
    for v, start, m in rows:
        print("%-8s mix %5.0f (x%.2f)   lead %5.0f (x%.2f)   bass %4.0f (x%.2f)   tail %5.1f" % (
            v["id"], m["bright"], m["bright"] / ref["bright"], m["lead"], m["lead"] / ref["lead"], m["bass"], m["bass"] / ref["bass"], m["tail"]))
    for v, start, m in rows[1:]:
        notes = []
        if abs(m["phone"] - ref["phone"]) > 1.5:
            notes.append("phone-speaker band moved %.1f dB" % (m["phone"] - ref["phone"]))
        if m["bands"][4] - ref["bands"][4] > 3.0 or m["bands"][5] - ref["bands"][5] > 3.0:
            notes.append("upper bands more than 3 dB above the reference: check for harshness")
        if m["bands"][0] - ref["bands"][0] > 2.0 or m["bands"][1] - ref["bands"][1] > 2.0:
            notes.append("low bands more than 2 dB above the reference: check for mud")
        if not 8.0 <= m["crest"] <= 14.0:
            notes.append("crest factor %.1f dB is outside 8-14" % m["crest"])
        print("guard rails %-7s %s" % (v["id"], "; ".join(notes) if notes else "all inside"))
    print("wrote", out)


if __name__ == "__main__":
    main()

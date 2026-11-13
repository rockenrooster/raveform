"""M2 audio checks: compare an engine render with the prototype's reference, and check onsets.

  python -I -B scripts/audio_check.py match <engine.wav> <engine.meta.json> <ref.json> [--stems <dir>]
  python -I -B scripts/audio_check.py onsets <engine.events.csv> <engine.meta.json>
  python -I -B scripts/audio_check.py tempo-onsets <engine.events.csv>      (M5: a render whose tempo changes)
  python -I -B scripts/audio_check.py metrics <file.wav>
  python -I -B scripts/audio_check.py audible-onsets <engine.wav> <engine.events.csv> <kind>

Tolerances (M2 brief): integrated loudness within 1 LU, each band within 1.5 dB, each section level
within 1.5 dB; true peak at most -1 dBFS; loudness -14 +/- 2 LUFS; every onset within one sample of
its grid position.
"""
import csv
import json
import math
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import audio_metrics as am  # noqa: E402

BANDS = am.BAND_NAMES


def load_json(path):
    with open(path, encoding="utf-8") as f:
        return json.load(f)


def measure_engine(wav, meta):
    x, rate = am.read_wav(wav)
    assert rate == am.SR, "48 kHz only"
    sections = [tuple(s) for s in meta.get("sections", [])]
    return am.measure(x, meta.get("bpm"), sections), x


def match(args):
    wav, meta_path, ref_path = args[:3]
    meta = load_json(meta_path)
    ref = load_json(ref_path)
    m, x = measure_engine(wav, meta)
    ok = True
    rows = []

    def row(name, eng, refv, tol, unit):
        nonlocal ok
        d = eng - refv
        good = abs(d) <= tol
        ok = ok and good
        rows.append((name, eng, refv, d, tol, "ok" if good else "FAIL", unit))

    row("loudness", m["lufs"], ref["lufs"], 1.0, "LUFS")
    for i, b in enumerate(BANDS):
        row("band " + b, m["bands_db"][i], ref["bands_db"][i], 1.5, "dB")
    for i, (s, r) in enumerate(zip(m.get("sections", []), ref.get("sections", []))):
        row("section %d %s" % (i + 1, s["section"]), s["rms_db"], r["rms_db"], 1.5, "dBFS")
    tp_ok = m["true_peak_db"] <= -1.0
    lufs_ok = abs(m["lufs"] + 14.0) <= 2.0
    ok = ok and tp_ok and lufs_ok
    print("%-22s %9s %9s %7s %5s" % ("", "engine", "proto", "diff", "tol"))
    for name, e, r, d, tol, verdict, unit in rows:
        print("%-22s %9.2f %9.2f %+7.2f %5.1f  %s %s" % (name, e, r, d, tol, unit, verdict))
    print("true peak %.2f dBFS (ref %.2f): %s; loudness %.2f LUFS (target -14 +/- 2): %s; phone band %.2f (ref %.2f); "
          "crest %.1f (ref %.1f); L/R corr %.2f (ref %.2f)" % (
              m["true_peak_db"], ref["true_peak_db"], "ok" if tp_ok else "FAIL", m["lufs"], "ok" if lufs_ok else "FAIL",
              m["phone_db"], ref["phone_db"], m["crest_db"], ref["crest_db"], m["lr_correlation"], ref["lr_correlation"]))
    if "--stems" in args:
        stems_dir = args[args.index("--stems") + 1]
        compare_stems(stems_dir, ref)
    out = {"engine": m, "reference": {k: ref[k] for k in ("lufs", "true_peak_db", "bands_db", "sections")}, "pass": ok}
    with open(os.path.splitext(wav)[0] + ".match.json", "w", encoding="utf-8") as f:
        json.dump(out, f, indent=1)
    print("MATCH", "PASS" if ok else "FAIL")
    return 0 if ok else 1


def compare_stems(stems_dir, ref):
    print("stem rms dB (engine / prototype; both before the master, the section gain and the normalisation):")
    names = sorted(ref["stems_rms_db"].keys())
    for name in names:
        path = os.path.join(stems_dir, name + ".f32")
        if not os.path.exists(path):
            continue
        s = np.fromfile(path, dtype="<f4").astype(np.float64).reshape(-1, 2)
        rms = 20 * math.log10(max(1e-9, float(np.sqrt(np.mean(s ** 2)))))
        bands = am.stem_bands({name: s})[name]
        rb = ref["stem_bands_db"][name]
        print("  %-11s %7.2f / %7.2f  (%+5.2f)   bands %s" % (
            name, rms, ref["stems_rms_db"][name], rms - ref["stems_rms_db"][name],
            " ".join("%+5.1f" % (a - b) for a, b in zip(bands, rb))))


def onsets(args):
    events_path, meta_path = args[:2]
    meta = load_json(meta_path)
    bpm, rate = meta["bpm"], meta["rate"]
    frames_per_tick = 60.0 * rate / (bpm * 96.0)
    worst, n, bad = 0.0, 0, 0
    kinds = {}
    with open(events_path, encoding="utf-8") as f:
        for row in csv.DictReader(f):
            tick = float(row["tick"])
            if tick < 0 or row["kind"] in ("tempo", "variant", "track"):
                continue
            # The grid position the prototype would use: int(round(t * SR)).
            grid = round(tick * frames_per_tick)
            d = int(row["frame"]) - grid
            worst = max(worst, abs(d))
            n += 1
            kinds[row["kind"]] = kinds.get(row["kind"], 0) + 1
            if abs(d) > 1:
                bad += 1
    print("onsets: %d checked (%s); worst |frame - grid| = %d samples; %d outside one sample" % (
        n, ", ".join("%s %d" % kv for kv in sorted(kinds.items())), worst, bad))
    print("ONSETS", "PASS" if bad == 0 and n > 0 else "FAIL")
    return 0 if bad == 0 and n > 0 else 1


def tempo_onsets(args):
    """M5: onsets of a render whose tempo changes (the set-list medley: ramps through each Track-change Break, the
    Build's 85% bar). The tempo map is rebuilt from the log's tempo rows as the main thread mirrors it (a segment at
    tick 0 starts the song at its frame; each later one starts on its tick, its frame carried on from the segment
    before at full precision); every onset must sit on round(frame of its tick). Also lists the tempo of each bar."""
    events_path = args[0]
    segs = []  # (frame, tick, ticks per frame)
    rows = []
    with open(events_path, encoding="utf-8") as f:
        for row in csv.DictReader(f):
            rows.append(row)
    worst, n, bad = 0, 0, 0
    kinds = {}
    for row in rows:
        tick, kind = float(row["tick"]), row["kind"]
        if kind == "tempo":
            tpf = float(row["x"])
            if int(row["a"]) == 1:
                segs.append((float(row["frame"]), tick, tpf))   # a hold (M4): explicit frames
            elif tick == 0.0:
                segs = [(float(row["frame"]), 0.0, tpf)]
            else:
                f0, t0, r0 = segs[-1]
                frame = f0 + (tick - t0) / r0
                if tick <= t0:
                    segs[-1] = (f0, t0, tpf)
                else:
                    segs.append((frame, tick, tpf))
            continue
        if tick < 0 or kind in ("variant", "track") or not segs:
            continue
        s = segs[0]
        for seg in segs:
            if seg[1] <= tick:
                s = seg
        grid = round(s[0] + (tick - s[1]) / s[2])
        d = int(row["frame"]) - grid
        worst = max(worst, abs(d))
        n += 1
        kinds[kind] = kinds.get(kind, 0) + 1
        if abs(d) > 1:
            bad += 1
    bpms = []
    for seg in segs:
        bpm = seg[2] * 60.0 * 48000.0 / 96.0
        bpms.append("bar %d: %.2f" % (int(seg[1] // 384), bpm))
    print("tempo map: %d segments; %s" % (len(segs), ", ".join(bpms)))
    print("onsets: %d checked (%s); worst |frame - grid| = %d samples; %d outside one sample" % (
        n, ", ".join("%s %d" % kv for kv in sorted(kinds.items())), worst, bad))
    print("ONSETS", "PASS" if bad == 0 and n > 0 else "FAIL")
    return 0 if bad == 0 and n > 0 else 1


def audible_onsets(args):
    """For a solo render: the first sample above a threshold after each logged onset of `kind` must be the
    logged frame itself (the sound really starts where the log says)."""
    wav, events_path, kind = args[:3]
    x, _ = am.read_wav(wav)
    mono = np.abs(x).max(axis=1)
    frames = []
    with open(events_path, encoding="utf-8") as f:
        for row in csv.DictReader(f):
            if row["kind"] == kind:
                frames.append(int(row["frame"]))
    worst = 0
    for fr in frames:
        seg = mono[max(0, fr - 64):fr + 64]
        quiet = seg[:64].max() if fr >= 64 else 0.0
        above = np.nonzero(seg[64:] > max(1e-4, quiet * 4))[0]
        if len(above):
            worst = max(worst, int(above[0]))
    print("audible onsets (%s): %d events, worst delay from the logged frame %d samples" % (kind, len(frames), worst))
    return 0


def metrics(args):
    x, _ = am.read_wav(args[0])
    m = am.measure(x)
    print(json.dumps(m, indent=1))
    return 0


def main():
    cmd, args = sys.argv[1], sys.argv[2:]
    return {"match": match, "onsets": onsets, "tempo-onsets": tempo_onsets, "metrics": metrics,
            "audible-onsets": audible_onsets}[cmd](args)


if __name__ == "__main__":
    sys.exit(main())

"""RAVEFORM sound-effect prototype (creative tooling, not game code).

Three renders for listening, built on the music prototype's DSP:
  sfx_sheet        every effect once, in order, with a printed timeline
  lead_timbres     the four players' lead sounds on the same phrase
  gameplay_mock    sixteen bars of Track 1 with a simulated run on top: kills aggregated on the 1/32
                   grid and pitched to the chord, Shard runs, Dash, Boost, a hit, a Nova

Usage:  python design/tools/sfxproto.py            -> design/music/previews/*.wav
"""
import copy
import json
import math
import os
import random
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import musicproto as mp  # noqa: E402

SR = mp.SR
PI = math.pi
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OUT = os.path.join(ROOT, "design", "music", "previews")

# Effect levels in the gameplay mix, relative to a music bed at -14 LUFS.
LEVEL = dict(kill_small=0.13, kill_medium=0.22, kill_large=0.40, shard=0.085, boost=0.30, upgrade=0.34,
             dash=0.22, perfect=0.20, hit=0.55, inhale=0.30, blast=0.70, spawn=0.10, tick=0.05,
             enemy_shot=0.07, elite=0.34)


def tt(seconds):
    n = int(seconds * SR)
    return n, np.arange(n) / SR


def sweep(f0, f1, tau, t):
    """Sine whose frequency glides exponentially from f0 to f1."""
    f = f1 + (f0 - f1) * np.exp(-t / tau)
    return np.sin(2 * PI * np.cumsum(f) / SR)


def lowpass_sweep(x, f0, f1, tau, q=1.0):
    t = np.arange(len(x)) / SR
    return mp.svf_lowpass(x[None, :], f1 + (f0 - f1) * np.exp(-t / tau), q)[0]


def bell(hz, dur=0.6, ratio=3.5, index=4.0):
    n, t = tt(dur)
    mod = np.sin(2 * PI * hz * ratio * t) * index * np.exp(-t / 0.15)
    x = np.sin(2 * PI * hz * t + mod) * np.exp(-t / (dur / 3.5)) * np.minimum(1.0, t / 0.002)
    return mp.fade_out(x)


# --------------------------------------------------------------------------- effects

def kill_small(hz):
    n, t = tt(0.13)
    x = sweep(hz * 1.6, hz, 0.012, t) * np.exp(-t / 0.045)
    click = mp.fft_filter(mp.white(n, int(hz)), mp.bp(2000.0, 7000.0)) * np.exp(-t / 0.010) * 0.5
    return mp.fade_out(mp.norm(np.tanh(1.3 * (x + click))))


def kill_medium(hz):
    n, t = tt(0.30)
    body = sweep(hz * 2.0, hz, 0.02, t) * np.exp(-t / 0.09)
    ring = body * np.sin(2 * PI * hz * 2.76 * t) * 0.3
    noise = mp.fft_filter(mp.white(n, int(hz) + 1), mp.bp(700.0, 4500.0)) * np.exp(-t / 0.06) * 0.6
    return mp.fade_out(mp.norm(np.tanh(1.4 * (body + ring + noise))))


def kill_large(hz):
    n, t = tt(0.75)
    sub = sweep(150.0, 55.0, 0.06, t) * np.exp(-t / 0.22)
    zap = lowpass_sweep(mp.saw([hz], n)[0], 3000.0, 300.0, 0.07, 1.4) * np.exp(-t / 0.16)
    crash = mp.fft_filter(mp.white(n, int(hz) + 2), mp.bp(500.0, 6500.0)) * np.exp(-t / 0.20) * 0.7
    return mp.fade_out(mp.norm(np.tanh(1.5 * (sub + 0.7 * zap + crash))), 0.03)


def shard(hz):
    n, t = tt(0.10)
    x = sweep(hz * 0.9, hz, 0.010, t) + 0.3 * sweep(hz * 1.8, hz * 2.0, 0.010, t)
    return mp.fade_out(mp.norm(x * np.exp(-t / 0.03) * np.minimum(1.0, t / 0.001)), 0.004)


def boost():
    out = np.zeros(int(1.1 * SR))
    for k, m in enumerate((69, 76, 81, 84)):
        b = bell(mp.midi_hz(m), 0.55)
        i = int(k * 0.055 * SR)
        out[i:i + len(b)] += b[:len(out) - i] * (0.8 + 0.07 * k)
    sparkle = bell(mp.midi_hz(100), 0.5, 2.0, 1.5) * 0.25
    i = int(0.24 * SR)
    out[i:i + len(sparkle)] += sparkle
    return mp.norm(out)


def upgrade():
    n, t = tt(1.3)
    f0 = np.array([mp.midi_hz(m) for m in (57, 60, 64, 67, 71)])
    x = (mp.saw(f0, n) + mp.saw(f0 * 2 ** (8 / 1200.0), n)).sum(axis=0) / 10.0
    x = lowpass_sweep(x, 2600.0, 900.0, 0.5, 0.9) * np.minimum(1.0, t / 0.08) * np.exp(-t / 0.45)
    for k, m in enumerate((76, 79, 83, 88)):
        b = bell(mp.midi_hz(m), 0.6) * 0.5
        i = int((0.10 + k * 0.07) * SR)
        x[i:i + len(b)] += b[:n - i]
    return mp.fade_out(mp.norm(x), 0.05)


def dash():
    n, t = tt(0.24)
    x = mp.white(n, 41) * np.minimum(1.0, t / 0.010) * np.exp(-t / 0.060)
    cutoff = 600.0 + 5400.0 * np.minimum(1.0, t / 0.11)
    y = mp.svf_lowpass(x[None, :], cutoff, 1.2)[0]
    return mp.fade_out(mp.norm(mp.fft_filter(y, mp.hp(400.0))))


def perfect_dash():
    d = dash()
    b = bell(1318.5, 0.75, 2.0, 2.0) * 0.6
    out = np.zeros(max(len(d), len(b)))
    out[:len(d)] += d
    out[:len(b)] += b
    return mp.norm(out)


def hit():
    n, t = tt(0.62)
    f = 55.0 + 165.0 * np.exp(-t / 0.10)
    raw = 2.0 * ((np.cumsum(f) / SR) % 1.0) - 1.0
    growl = np.tanh(3.0 * lowpass_sweep(raw, 2500.0, 200.0, 0.12, 1.6)) * np.exp(-t / 0.20)
    noise = mp.fft_filter(mp.white(n, 43), mp.bp(300.0, 5000.0)) * np.exp(-t / 0.08)
    thud = sweep(120.0, 45.0, 0.05, t) * np.exp(-t / 0.18)
    return mp.fade_out(mp.norm(0.7 * growl + 0.6 * noise + 0.8 * thud), 0.03)


def nova_inhale(dur):
    n, t = tt(dur)
    noise = mp.fft_filter(mp.white(n, 44), mp.hp(1500.0)) * np.exp((t - dur) / (dur * 0.3))
    rise = np.sin(2 * PI * np.cumsum(180.0 + 720.0 * (t / dur) ** 2) / SR) * (t / dur) ** 2 * 0.4
    return mp.fade_out(mp.norm(noise + rise), 0.006)


def nova_blast():
    imp = mp.make_impact()
    n, t = len(imp), np.arange(len(imp)) / SR
    burst = mp.fft_filter(mp.white(n, 45), mp.bp(200.0, 8000.0)) * np.exp(-t / 0.12) * 0.8
    return mp.norm(imp + burst)


def spawn(hz, dur=0.5):
    n, t = tt(dur)
    x = (np.sin(2 * PI * hz * t) + 0.3 * np.sin(2 * PI * hz * 2 * t)) * (t / dur) ** 3
    return mp.fade_out(mp.norm(x), 0.004)


def tick():
    n, t = tt(0.03)
    return mp.fade_out(np.sin(2 * PI * 3000.0 * t) * np.exp(-t / 0.006), 0.003)


def enemy_shot():
    n, t = tt(0.09)
    x = mp.fft_filter(mp.square([180.0], n)[0], mp.lp(1500.0)) * np.exp(-t / 0.03)
    return mp.fade_out(mp.norm(x), 0.004)


def elite_spawn():
    n, t = tt(1.3)
    f0 = np.array([mp.midi_hz(45), mp.midi_hz(52)])
    x = (mp.saw(f0, n) + mp.saw(f0 * 2 ** (12 / 1200.0), n)).sum(axis=0) / 4.0
    x = lowpass_sweep(x, 1400.0, 500.0, 0.3, 1.2) * np.minimum(1.0, t / 0.02) * np.exp(-t / 0.5)
    x += 0.5 * np.sin(2 * PI * mp.midi_hz(33) * t) * np.exp(-t / 0.4)
    return mp.fade_out(mp.norm(np.tanh(1.6 * x)), 0.05)


def two_note(hz_a, hz_b, d_a, d_b, soft=False):
    na, ta = tt(d_a)
    nb, tb = tt(d_b)
    a = np.sin(2 * PI * hz_a * ta) * np.exp(-ta / (d_a * 0.6))
    b = np.sin(2 * PI * hz_b * tb) * np.exp(-tb / (d_b * 0.5))
    x = np.concatenate([mp.fade_out(a, 0.004), mp.fade_out(b, 0.006)])
    if soft:
        x = x + 0.25 * np.concatenate([np.sin(2 * PI * 2 * hz_a * ta) * np.exp(-ta / (d_a * 0.4)),
                                        np.sin(2 * PI * 2 * hz_b * tb) * np.exp(-tb / (d_b * 0.4))])
    return mp.norm(x)


def ui_tick():
    n, t = tt(0.03)
    return mp.fade_out(np.sin(2 * PI * 1800.0 * t) * np.exp(-t / 0.008), 0.003)


def game_over():
    out = np.zeros(int(2.6 * SR))
    for k, m in enumerate((81, 76, 72, 69, 64, 57)):
        b = bell(mp.midi_hz(m), 0.9, 2.0, 2.5)
        i = int(k * 0.13 * SR)
        out[i:i + len(b)] += b[:len(out) - i] * (1.0 - 0.08 * k)
    n, t = tt(1.4)
    boom = sweep(90.0, 35.0, 0.12, t) * np.exp(-t / 0.45)
    i = int(0.78 * SR)
    out[i:i + n] += boom * 0.9
    return mp.fade_out(mp.norm(out), 0.1)


# --------------------------------------------------------------------------- lead timbres

def lead_p1(notes):
    return mp.render_plucks(notes, mp.PATCH["lead"])


def lead_p2(notes):   # hollow square pluck
    p = mp.PATCH["lead"]
    n = int(p["length"] * SR)
    f0 = np.array([mp.midi_hz(m) for m, _ in notes])
    x = mp.square(f0, n) + mp.square(f0 * 2 ** (6 / 1200.0), n)
    t = np.arange(n) / SR
    cutoff = (700.0 + 3100.0 * np.exp(-t / 0.060))[None, :] * ((f0 / 440.0) ** 0.5)[:, None]
    y = mp.svf_lowpass(x * 0.42, cutoff, 1.8)
    y *= (np.minimum(1.0, t / 0.002) * np.exp(-t / 0.15))[None, :] * np.array([v for _, v in notes])[:, None]
    y[:, -240:] *= np.linspace(1.0, 0.0, 240)[None, :]
    return y


def lead_p3(notes):   # bell
    n = int(0.36 * SR)
    out = np.zeros((len(notes), n))
    for i, (m, v) in enumerate(notes):
        b = bell(mp.midi_hz(m), 0.36, 2.0, 2.4)
        out[i, :len(b)] = b[:n] * v * 0.8
    return out


def lead_p4(notes):   # wide detuned stab
    n = int(0.30 * SR)
    f0 = np.array([mp.midi_hz(m) for m, _ in notes])
    x = sum(mp.saw(f0 * 2 ** (c / 1200.0), n) for c in (-12.0, 0.0, 12.0)) / 3.0 + 0.3 * mp.saw(f0 * 2.0, n)
    t = np.arange(n) / SR
    cutoff = (900.0 + 2400.0 * np.exp(-t / 0.050))[None, :] * ((f0 / 440.0) ** 0.5)[:, None]
    y = mp.svf_lowpass(x * 0.6, cutoff, 1.4)
    y *= (np.minimum(1.0, t / 0.002) * np.exp(-t / 0.12))[None, :] * np.array([v for _, v in notes])[:, None]
    y[:, -240:] *= np.linspace(1.0, 0.0, 240)[None, :]
    return y


# --------------------------------------------------------------------------- assembly

def place(buf, x, t, gain=1.0, pan=0.0):
    i = int(round(t * SR))
    if i < 0 or i >= len(buf):
        return
    m = min(len(x), len(buf) - i)
    a = (pan + 1.0) * PI / 4.0
    buf[i:i + m, 0] += x[:m] * gain * math.cos(a) * 1.4142
    buf[i:i + m, 1] += x[:m] * gain * math.sin(a) * 1.4142


def finish(buf, path, peak_db=-1.0):
    buf = np.tanh(buf)
    peak = np.max(np.abs(buf))
    limit = 10 ** (peak_db / 20.0)
    if peak > limit:
        buf *= limit / peak
    mp.write_wav(path, buf)
    print("  %s  %.1f s  lufs %.1f  peak %.1f dBFS" % (os.path.basename(path), len(buf) / SR, mp.lufs(buf), 20 * math.log10(np.max(np.abs(buf)))))


def sfx_sheet():
    items = [
        ("kill, small (three pitches)", [(kill_small(880.0), 0.0), (kill_small(1046.5), 0.18), (kill_small(1318.5), 0.36)], 0.5),
        ("kill, medium", [(kill_medium(440.0), 0.0)], 0.6),
        ("kill, large", [(kill_large(220.0), 0.0)], 0.7),
        ("Shard run (eight, rising)", [(shard(mp.midi_hz(m)), k * 0.0625) for k, m in enumerate((81, 84, 86, 88, 91, 93, 96, 98))], 0.35),
        ("Boost picked up", [(boost(), 0.0)], 0.6),
        ("upgrade taken", [(upgrade(), 0.0)], 0.6),
        ("Dash", [(dash(), 0.0)], 0.6),
        ("Perfect Dash", [(perfect_dash(), 0.0)], 0.6),
        ("hit taken", [(hit(), 0.0)], 0.7),
        ("Nova: inhale, then blast on the beat", [(nova_inhale(0.5), 0.0), (nova_blast(), 0.5)], 0.7),
        ("enemy warp-in (two)", [(spawn(220.0), 0.0), (spawn(329.6), 0.5)], 0.4),
        ("telegraph tick, then enemy shot (x3)", [(tick(), 0.0), (enemy_shot(), 0.25), (tick(), 0.5), (enemy_shot(), 0.75), (tick(), 1.0), (enemy_shot(), 1.25)], 0.5),
        ("elite arrives", [(elite_spawn(), 0.0)], 0.6),
        ("interface: tick, confirm, back", [(ui_tick(), 0.0), (two_note(1318.5, 1760.0, 0.04, 0.09), 0.35), (two_note(1760.0, 1318.5, 0.04, 0.09), 0.85)], 0.5),
        ("Shield cell restored", [(two_note(659.3, 880.0, 0.07, 0.16, soft=True), 0.0)], 0.5),
        ("game over", [(game_over(), 0.0)], 0.6),
    ]
    total = sum(max(off + len(x) / SR for x, off in parts) + 0.7 for _, parts, _ in items) + 0.5
    buf = np.zeros((int(total * SR), 2))
    t = 0.3
    print("sfx sheet timeline:")
    for name, parts, gain in items:
        print("  %5.1f s  %s" % (t, name))
        for x, off in parts:
            place(buf, x, t + off, gain)
        t += max(off + len(x) / SR for x, off in parts) + 0.7
    finish(buf, os.path.join(OUT, "sfx_sheet.wav"))


def lead_timbres(track):
    step = 60.0 / track["bpm"] / 4.0
    kick = mp.make_kick()
    names = [("P1 saw pluck", lead_p1), ("P2 square pluck", lead_p2), ("P3 bell", lead_p3), ("P4 wide stab", lead_p4)]
    buf = np.zeros((int((len(names) * 4.5 + 0.5) * SR), 2))
    print("lead timbres timeline:")
    for k, (name, fn) in enumerate(names):
        t0 = 0.3 + k * 4.5
        print("  %5.1f s  %s" % (t0, name))
        notes, times = [], []
        for b, chord in enumerate(("Am", "F")):
            for s in range(16):
                notes.append((track["chords"][chord]["arp"][s], 1.0 if s % 4 == 0 else 0.78))
                times.append(t0 + (b * 16 + s) * step)
            for beat in range(4):
                place(buf, kick, t0 + (b * 16 + beat * 4) * step, 0.35)
        y = fn(notes)
        for i in range(len(notes)):
            place(buf, y[i], times[i], 0.34, 0.2 * math.sin(i * 0.9))
    finish(buf, os.path.join(OUT, "lead_timbres.wav"))


def gameplay_mock(track):
    rng = random.Random(7)
    t = copy.deepcopy(track)
    full = {"kick": 1, "hat_off": 1, "bass": "groove", "lead": 1, "clap": 1, "stab": 1, "pulse": 1, "shaker": 1, "pad": 0.6}
    drop = {"kick": 1, "clap": 1, "hat_16": 1, "ohat": 1, "bass": "drop", "lead": 1, "stab": 1, "pulse": 1, "shaker": 1, "pad": 1.0}
    t["arrangement"] = [
        {"section": "groove", "bars": 4, "layers": full},
        {"section": "build", "bars": 4, "layers": {"kick": 1, "hat_off": 1, "bass": "groove", "lead": 1, "pad": 0.8, "stab": 1}},
        {"section": "drop", "bars": 4, "layers": dict(drop, lead="double")},
        {"section": "drop", "bars": 4, "layers": drop},
    ]
    bed, _, info = mp.render(t)
    bar = 240.0 / track["bpm"]
    step32 = bar / 32.0
    sfx = np.zeros_like(bed)
    duck = np.ones(len(bed))

    def chord_at(b):
        return track["chords"][track["progression"][b % len(track["progression"])]]

    def dip(time, depth, tau):
        i = int(time * SR)
        n = min(len(duck) - i, int(tau * 6 * SR))
        if n > 0:
            duck[i:i + n] = np.minimum(duck[i:i + n], 1.0 - depth * np.exp(-np.arange(n) / (tau * SR)))

    events = []
    shard_scale = (81, 84, 86, 88, 91, 93, 96, 98, 100, 103)
    for b in range(16):
        section = "groove" if b < 4 else ("build" if b < 8 else "drop")
        p_small = {"groove": 0.22, "build": 0.30 + 0.05 * (b - 4), "drop": 0.55}[section]
        p_medium = {"groove": 0.18, "build": 0.30, "drop": 0.45}[section]
        tones = chord_at(b)["stab"]
        for s in range(32):
            when = b * bar + s * step32
            if b == 7 and s >= 16:
                continue                                   # the Build's last half bar is the inhale
            if rng.random() < p_small:
                count = 1 + int(rng.expovariate(1.0 / {"groove": 0.8, "build": 1.6, "drop": 3.5}[section]))
                gain = LEVEL["kill_small"] * (1.0 - math.exp(-count / 2.5)) * 1.6
                sfx_x = kill_small(mp.midi_hz(rng.choice(tones) + 12))
                place(sfx, sfx_x, when, gain, rng.uniform(-0.6, 0.6))
            if s % 4 == 0 and rng.random() < p_medium:
                place(sfx, kill_medium(mp.midi_hz(rng.choice(tones))), when, LEVEL["kill_medium"], rng.uniform(-0.5, 0.5))
                run = rng.randint(3, 8)
                for k in range(run):
                    place(sfx, shard(mp.midi_hz(shard_scale[k])), when + (2 + k) * step32, LEVEL["shard"], rng.uniform(-0.3, 0.3))
        if section == "drop" and b in (9, 11, 13, 14):
            place(sfx, kill_large(mp.midi_hz(chord_at(b)["stab"][0] - 12)), b * bar + bar * 0.5, LEVEL["kill_large"])
            dip(b * bar + bar * 0.5, 0.25, 0.12)
        for beat in (0, 2):                                 # warp-ins land on beats 1 and 3
            if not (b == 7 and beat == 2):
                place(sfx, spawn(mp.midi_hz(chord_at(b)["bass"] + 12)), b * bar + beat * bar / 4 - 0.5, LEVEL["spawn"], rng.uniform(-0.7, 0.7))
        if section == "drop":
            for beat in (1, 3):
                place(sfx, tick(), b * bar + beat * bar / 4 - bar / 4, LEVEL["tick"], 0.5)
                place(sfx, enemy_shot(), b * bar + beat * bar / 4, LEVEL["enemy_shot"], 0.5)

    events.append((1 * bar + 3 * bar / 4, "Boost picked up"))
    place(sfx, boost(), 1 * bar + 3 * bar / 4, LEVEL["boost"])
    events.append((2 * bar + 2 * bar / 4, "Perfect Dash (on the beat)"))
    place(sfx, perfect_dash(), 2 * bar + 2 * bar / 4, LEVEL["perfect"] + LEVEL["dash"] * 0.5)
    events.append((5 * bar + 1.5 * bar / 4, "hit taken (music ducks)"))
    place(sfx, hit(), 5 * bar + 1.5 * bar / 4, LEVEL["hit"])
    dip(5 * bar + 1.5 * bar / 4, 0.5, 0.18)
    events.append((8 * bar, "the Drop"))
    events.append((10 * bar + 1.3 * bar / 4, "Dash (off the beat)"))
    place(sfx, dash(), 10 * bar + 1.3 * bar / 4, LEVEL["dash"])
    events.append((12 * bar, "elite arrives"))
    place(sfx, elite_spawn(), 12 * bar, LEVEL["elite"])
    press = 13 * bar + 2.3 * bar / 4
    land = 13 * bar + 3 * bar / 4
    events.append((press, "Nova pressed: inhale"))
    events.append((land, "Nova lands on the next beat"))
    place(sfx, nova_inhale(land - press), press, LEVEL["inhale"])
    place(sfx, nova_blast(), land, LEVEL["blast"])
    dip(land, 0.45, 0.25)

    wet = mp.reverb(sfx * 0.18, mp.PATCH["reverb"])
    mix = bed * duck[:, None] + sfx + wet
    print("gameplay mock timeline:")
    for when, name in sorted(events):
        print("  %5.1f s  %s" % (when, name))
    finish(mix, os.path.join(OUT, "gameplay_mock.wav"))
    print("  bed alone lufs %.1f, effects alone lufs %.1f" % (mp.lufs(bed), mp.lufs(sfx + wet)))


def main():
    os.makedirs(OUT, exist_ok=True)
    track = json.load(open(os.path.join(ROOT, "design", "music", "track01.json"), encoding="utf-8"))
    sfx_sheet()
    lead_timbres(track)
    gameplay_mock(track)


if __name__ == "__main__":
    main()

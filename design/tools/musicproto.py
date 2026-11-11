"""RAVEFORM music prototype (creative tooling, not game code).

Renders a Track definition (design/music/*.json) to a WAV with the same synthesis recipes the engine
will use, so composition and sound design can be judged by ear long before the audio engine exists.
The DSP here is the reference for the C++ port: band-limited oscillators (PolyBLEP), a zero-delay
state-variable low-pass (Cytomic/Simper form), exponential envelopes, kick-keyed sidechain, tempo-synced
ping-pong delay, a short reverb, and a soft-clip master.

Usage:  python design/tools/musicproto.py design/music/track01.json [out.wav]
Requires: numpy. (ffmpeg is used by the wrapper script for loudness and spectrograms.)
"""
import json
import math
import os
import sys
import wave

import numpy as np

SR = 48000
PI = math.pi

# Mix levels and patch parameters: the creative knobs. Mirror any change in design/music/README.md.
MIX = dict(kick=0.62, clap=0.95, snare=0.40, hat=0.11, ohat=0.13, shaker=0.07, bass=0.68, lead=0.38,
           pad=0.50, stab=0.75, pulse=0.36, riser=0.26, impact=0.70, swell=0.22)
# Bed level per section, applied before the master soft clip. The dip at the end of the Build is what
# makes the Drop land.
SECTION_GAIN = dict(groove=0.84, build=(0.84, 0.66), drop=1.0)
SECTION_GAIN["break"] = 0.92
TARGET_LUFS = -14.0   # integrated loudness of the music bed; leaves headroom for effects in game
SEND = dict(lead=0.22, stab=0.30, clap=0.22, snare=0.25, pad=0.30, pulse=0.20)   # reverb sends
DUCK = dict(bass=0.80, pad=0.55, stab=0.30, lead=0.22, pulse=0.0)                # sidechain depth
PATCH = dict(
    lead=dict(detune_cents=7.0, sub=0.18, fc_start=4200.0, fc_end=750.0, fc_tau=0.070, q=2.2,
              attack=0.002, decay=0.16, length=0.36, keytrack=0.5),
    # Low resonance on purpose: with q above ~1 the closed filter rings at fc_end for the whole note.
    bass=dict(square_mix=0.5, sub=0.35, fc_start=2400.0, fc_end=360.0, fc_tau=0.090, q=0.8,
              attack=0.003, decay=0.22, release=0.03, drive=1.5),
    stab=dict(detune_cents=9.0, fc_start=3600.0, fc_end=900.0, fc_tau=0.055, q=1.5,
              attack=0.002, decay=0.11, length=0.26),
    pad=dict(detune_cents=6.0, fc_base=950.0, fc_lfo=650.0, lfo_hz=0.125, q=0.9,
             attack=0.35, release=0.7, highpass=220.0),
    delay=dict(steps=3.0, feedback=0.36, mix=0.22, repeats=7),     # dotted eighth, ping-pong
    reverb=dict(rt60=1.4, predelay=0.015, highpass=320.0, lowpass=7000.0, mix=1.0),
    sidechain_tau=0.11,
    master_drive=1.25,
)


def midi_hz(m):
    return 440.0 * 2.0 ** ((m - 69) / 12.0)


def rng(seed):
    return np.random.default_rng(seed)


def white(n, seed):
    return rng(seed).standard_normal(n)


def fft_filter(x, gain_fn):
    """Static zero-phase filter: multiply the spectrum by gain_fn(frequencies_hz)."""
    n = len(x)
    X = np.fft.rfft(x)
    f = np.fft.rfftfreq(n, 1.0 / SR)
    return np.fft.irfft(X * gain_fn(f), n)


def hp(fc, order=2):
    return lambda f: 1.0 / np.sqrt(1.0 + (fc / np.maximum(f, 1e-6)) ** (2 * order))


def lp(fc, order=2):
    return lambda f: 1.0 / np.sqrt(1.0 + (f / fc) ** (2 * order))


def bp(lo, hi, order=2):
    a, b = hp(lo, order), lp(hi, order)
    return lambda f: a(f) * b(f)


def norm(x, peak=1.0):
    m = np.max(np.abs(x))
    return x * (peak / m) if m > 0 else x


def fade_out(x, seconds=0.008):
    n = min(len(x), int(seconds * SR))
    x[-n:] *= np.linspace(1.0, 0.0, n)
    return x


# --------------------------------------------------------------------------- oscillators and filter

def polyblep(t, dt):
    out = np.zeros_like(t)
    m = t < dt
    x = t[m] / dt[m]
    out[m] = x + x - x * x - 1.0
    m = t > 1.0 - dt
    x = (t[m] - 1.0) / dt[m]
    out[m] = x * x + x + x + 1.0
    return out


def saw(freqs, n, phase0=None):
    """Band-limited sawtooth. freqs: (rows,) Hz. Returns (rows, n)."""
    freqs = np.asarray(freqs, np.float64)[:, None]
    dt = np.broadcast_to(freqs / SR, (freqs.shape[0], n)).copy()
    p0 = np.zeros((freqs.shape[0], 1)) if phase0 is None else np.asarray(phase0, np.float64)[:, None]
    t = (p0 + dt * np.arange(n)[None, :]) % 1.0
    return 2.0 * t - 1.0 - polyblep(t, dt)


def square(freqs, n):
    freqs = np.asarray(freqs, np.float64)[:, None]
    dt = np.broadcast_to(freqs / SR, (freqs.shape[0], n)).copy()
    t = (dt * np.arange(n)[None, :]) % 1.0
    return np.where(t < 0.5, 1.0, -1.0) + polyblep(t, dt) - polyblep((t + 0.5) % 1.0, dt)


def svf_lowpass(X, cutoff, q):
    """Zero-delay state-variable low-pass applied along time for every row at once.
    X: (rows, n). cutoff: (n,) or (rows, n) in Hz, may vary per sample. q: resonance (0.5 .. 10)."""
    rows, n = X.shape
    g = np.tan(PI * np.clip(cutoff, 20.0, SR * 0.45) / SR)
    k = 1.0 / q
    a1 = 1.0 / (1.0 + g * (g + k))
    a2 = g * a1
    a3 = g * a2
    two_d = a1.ndim == 2
    if two_d:
        a1, a2, a3 = a1.T.copy(), a2.T.copy(), a3.T.copy()
    Xt = X.T.copy()
    Y = np.empty_like(Xt)
    ic1 = np.zeros(rows)
    ic2 = np.zeros(rows)
    for i in range(n):
        A1, A2, A3 = a1[i], a2[i], a3[i]
        v3 = Xt[i] - ic2
        v1 = A1 * ic1 + A2 * v3
        v2 = ic2 + A2 * ic1 + A3 * v3
        ic1 = 2.0 * v1 - ic1
        ic2 = 2.0 * v2 - ic2
        Y[i] = v2
    return Y.T


# --------------------------------------------------------------------------- one-shots (drum synth)

def make_kick():
    n = int(0.42 * SR)
    t = np.arange(n) / SR
    # The sweep starts high so the knock survives on phone speakers, which reproduce little below 250 Hz.
    f = 55.0 + 240.0 * np.exp(-t / 0.018) + 30.0 * np.exp(-t / 0.10)
    body = np.sin(2 * PI * np.cumsum(f) / SR) * np.exp(-t / 0.115) * np.minimum(1.0, t / 0.0015)
    click = fft_filter(white(n, 1), bp(1800.0, 6000.0)) * np.exp(-t / 0.005) * 0.7
    return fade_out(norm(np.tanh(1.8 * (body + click))))


def make_clap():
    n = int(0.34 * SR)
    t = np.arange(n) / SR
    noise = fft_filter(white(n, 2), bp(750.0, 3600.0))
    env = np.zeros(n)
    for d in (0.0, 0.011, 0.021):
        env += (t >= d) * np.exp(-np.maximum(t - d, 0.0) / 0.006) * 0.8
    env += (t >= 0.030) * np.exp(-np.maximum(t - 0.030, 0.0) / 0.085)
    return fade_out(norm(noise * env))


def make_snare():
    n = int(0.24 * SR)
    t = np.arange(n) / SR
    f = 185.0 + 140.0 * np.exp(-t / 0.012)
    tone = np.sin(2 * PI * np.cumsum(f) / SR) * np.exp(-t / 0.07)
    noise = fft_filter(white(n, 3), bp(1800.0, 8500.0)) * np.exp(-t / 0.09)
    return fade_out(norm(np.tanh(1.4 * (0.6 * tone + 0.9 * noise))))


def make_hat(decay, seed):
    n = int((decay * 6.0 + 0.02) * SR)
    t = np.arange(n) / SR
    metal = np.zeros(n)
    for f in (3136.0, 4410.0, 5274.0, 6211.0, 7102.0, 8372.0):
        metal += np.where((f * t) % 1.0 < 0.5, 1.0, -1.0)
    x = 0.6 * fft_filter(metal, hp(6500.0, 3)) / 3.0 + 0.4 * fft_filter(white(n, seed), hp(7000.0, 3))
    return fade_out(norm(x * np.exp(-t / decay)), 0.004)


def make_shaker():
    n = int(0.09 * SR)
    t = np.arange(n) / SR
    x = fft_filter(white(n, 9), bp(4500.0, 11000.0)) * np.minimum(1.0, t / 0.006) * np.exp(-t / 0.022)
    return fade_out(norm(x), 0.004)


def make_pulse():
    n = int(0.36 * SR)
    t = np.arange(n) / SR
    f = 170.0 + 650.0 * np.exp(-t / 0.045)
    x = np.sin(2 * PI * np.cumsum(f) / SR) * np.exp(-t / 0.13)
    x += 0.25 * fft_filter(white(n, 5), bp(1500.0, 5000.0)) * np.exp(-t / 0.03)
    return fade_out(norm(np.tanh(1.5 * x)))


def make_impact():
    n = int(2.4 * SR)
    t = np.arange(n) / SR
    f = 38.0 + 82.0 * np.exp(-t / 0.25)
    sub = np.sin(2 * PI * np.cumsum(f) / SR) * np.exp(-t / 0.7)
    boom = fft_filter(white(n, 6), bp(140.0, 650.0)) * np.exp(-t / 0.28) * 0.55
    crash = fft_filter(white(n, 7), bp(3500.0, 11000.0)) * np.exp(-t / 0.65) * 0.30
    return fade_out(norm(np.tanh(1.3 * (sub + boom)) + crash), 0.05)


def make_swell(seconds):
    n = int(seconds * SR)
    t = np.arange(n) / SR
    x = fft_filter(white(n, 8), hp(2500.0)) * np.exp((t - seconds) / (seconds * 0.28))
    return fade_out(norm(x), 0.01)


def make_riser(seconds):
    n = int(seconds * SR)
    t = np.arange(n) / SR
    u = t / seconds
    out = np.zeros((n, 2))
    centres = np.geomspace(300.0, 9000.0, 12)
    for ch in range(2):
        acc = np.zeros(n)
        for k, fc in enumerate(centres):
            band = fft_filter(white(n, 100 + 17 * k + ch), bp(fc * 0.8, fc * 1.25))
            gate = np.clip((u - k / 13.0) * 4.0, 0.0, 1.0)
            acc += band * gate * gate * (3.0 - 2.0 * gate)
        out[:, ch] = acc * u ** 1.6
    return out / np.max(np.abs(out))


# --------------------------------------------------------------------------- pitched voices (batched)

def render_plucks(notes, patch, osc="saw"):
    """notes: list of (midi, velocity). Returns (len(notes), n) mono signals."""
    n = int(patch["length"] * SR)
    f0 = np.array([midi_hz(m) for m, _ in notes])
    det = 2.0 ** (patch["detune_cents"] / 1200.0)
    r = rng(11)
    x = saw(f0, n, r.random(len(notes))) + saw(f0 * det, n, r.random(len(notes)))
    if patch.get("sub"):
        x += patch["sub"] * square(f0 * 0.5, n)
    t = np.arange(n) / SR
    track = (f0 / 440.0) ** patch.get("keytrack", 0.0)
    cutoff = (patch["fc_end"] + (patch["fc_start"] - patch["fc_end"]) * np.exp(-t / patch["fc_tau"]))[None, :] * track[:, None]
    y = svf_lowpass(x * 0.5, cutoff, patch["q"])
    env = np.minimum(1.0, t / patch["attack"]) * np.exp(-t / patch["decay"])
    y = y * env[None, :] * np.array([v for _, v in notes])[:, None]
    y[:, -240:] *= np.linspace(1.0, 0.0, 240)[None, :]
    return y


def render_bass(notes, patch, step):
    """notes: list of (midi, velocity, length_steps)."""
    longest = max(l for _, _, l in notes)
    n = int((longest * step + 0.12) * SR)
    f0 = np.array([midi_hz(m) for m, _, _ in notes])
    x = (1.0 - patch["square_mix"]) * saw(f0, n) + patch["square_mix"] * square(f0, n)
    t = np.arange(n) / SR
    cutoff = patch["fc_end"] + (patch["fc_start"] - patch["fc_end"]) * np.exp(-t / patch["fc_tau"])
    y = svf_lowpass(x, cutoff, patch["q"])
    y = y + patch["sub"] * np.sin(2 * PI * (f0[:, None] * 0.5) * t[None, :])
    gate = np.array([l * step for _, _, l in notes])[:, None]
    env = np.minimum(1.0, t[None, :] / patch["attack"]) * np.exp(-t[None, :] / patch["decay"])
    env = env * np.where(t[None, :] < gate, 1.0, np.exp(-(t[None, :] - gate) / patch["release"]))
    y = np.tanh(patch["drive"] * y * env) * np.array([v for _, v, _ in notes])[:, None]
    y[:, -240:] *= np.linspace(1.0, 0.0, 240)[None, :]
    return y


def render_stabs(chords, patch):
    """chords: list of (midi_notes, velocity)."""
    n = int(patch["length"] * SR)
    det = 2.0 ** (patch["detune_cents"] / 1200.0)
    rows = []
    for notes, _ in chords:
        f0 = np.array([midi_hz(m) for m in notes])
        rows.append((saw(f0, n) + saw(f0 * det, n)).sum(axis=0) / (2.0 * len(notes)))
    x = np.array(rows)
    t = np.arange(n) / SR
    cutoff = patch["fc_end"] + (patch["fc_start"] - patch["fc_end"]) * np.exp(-t / patch["fc_tau"])
    y = svf_lowpass(x, cutoff, patch["q"])
    env = np.minimum(1.0, t / patch["attack"]) * np.exp(-t / patch["decay"])
    y = y * env[None, :] * np.array([v for _, v in chords])[:, None]
    y[:, -240:] *= np.linspace(1.0, 0.0, 240)[None, :]
    return y


def render_pads(chords, patch, hold):
    """chords: list of (midi_notes, level, brightness). Returns (len, n, 2) stereo."""
    n = int((hold + patch["release"] * 4.0) * SR)
    t = np.arange(n) / SR
    out = np.zeros((len(chords), n, 2))
    for ch, sign in ((0, 1.0), (1, -1.0)):
        rows = []
        for notes, _, _ in chords:
            f0 = np.array([midi_hz(m) for m in notes])
            det = 2.0 ** (sign * patch["detune_cents"] / 1200.0)
            r = rng(31 + ch)
            rows.append((saw(f0, n, r.random(len(notes))) + saw(f0 * det, n, r.random(len(notes)))).sum(axis=0) / (2.0 * len(notes)))
        x = np.array(rows)
        bright = np.array([b for _, _, b in chords])[:, None]
        lfo = 0.5 + 0.5 * np.sin(2 * PI * patch["lfo_hz"] * t + ch * 0.9)
        cutoff = (patch["fc_base"] + patch["fc_lfo"] * lfo)[None, :] * bright
        y = svf_lowpass(x, cutoff, patch["q"])
        env = np.minimum(1.0, t / patch["attack"]) * np.where(t < hold, 1.0, np.exp(-(t - hold) / patch["release"]))
        y = y * env[None, :] * np.array([lv for _, lv, _ in chords])[:, None]
        for i in range(len(chords)):
            out[i, :, ch] = fft_filter(y[i], hp(patch["highpass"]))
    out[:, -480:, :] *= np.linspace(1.0, 0.0, 480)[None, :, None]
    return out


# --------------------------------------------------------------------------- mixing helpers

class Mixer:
    def __init__(self, seconds):
        self.n = int(seconds * SR)
        self.buses = {}

    def bus(self, name):
        if name not in self.buses:
            self.buses[name] = np.zeros((self.n, 2))
        return self.buses[name]

    def place(self, name, x, t, gain=1.0, pan=0.0):
        b = self.bus(name)
        i = int(round(t * SR))
        if i >= self.n or i < 0:
            return
        m = min(len(x), self.n - i)
        if x.ndim == 1:
            a = (pan + 1.0) * PI / 4.0
            b[i:i + m, 0] += x[:m] * gain * math.cos(a) * 1.4142
            b[i:i + m, 1] += x[:m] * gain * math.sin(a) * 1.4142
        else:
            b[i:i + m] += x[:m] * gain


def ping_pong(x, delay_s, feedback, mix, repeats):
    out = x.copy()
    d = int(delay_s * SR)
    for k in range(1, repeats + 1):
        g = mix * feedback ** (k - 1)
        if k * d >= len(x):
            break
        src = x[:len(x) - k * d].mean(axis=1)
        out[k * d:, k % 2] += src * g * 1.3
        out[k * d:, (k + 1) % 2] += src * g * 0.35
    return out


def reverb(x, p):
    n_ir = int(p["rt60"] * 1.3 * SR)
    t = np.arange(n_ir) / SR
    out = np.zeros_like(x)
    for ch in range(2):
        ir = white(n_ir, 50 + ch) * 10.0 ** (-3.0 * t / p["rt60"])
        ir = fft_filter(ir, bp(p["highpass"], p["lowpass"], 1))
        ir = np.concatenate([np.zeros(int(p["predelay"] * SR)), ir])
        ir /= np.sqrt(np.sum(ir * ir))
        m = len(x) + len(ir)
        size = 1 << (m - 1).bit_length()
        mono = x.mean(axis=1) * 0.6 + x[:, ch] * 0.4
        y = np.fft.irfft(np.fft.rfft(mono, size) * np.fft.rfft(ir, size), size)
        out[:, ch] = y[:len(x)]
    return out * p["mix"]


# --------------------------------------------------------------------------- arrangement

def render(track):
    bpm = track["bpm"]
    beat = 60.0 / bpm
    step = beat / 4.0
    bar = 4.0 * beat
    arr = track["arrangement"]
    total_bars = sum(s["bars"] for s in arr)
    mx = Mixer(total_bars * bar + 2.5)
    pat = track["patterns"]
    prog = track["progression"]
    chords = track["chords"]

    kick, clap, snare = make_kick(), make_clap(), make_snare()
    hat, ohat, shaker, pulse = make_hat(0.030, 21), make_hat(0.17, 22), make_shaker(), make_pulse()
    impact = make_impact()

    kick_times = []
    lead_notes, lead_times, lead_pans = [], [], []
    bass_notes, bass_times = [], []
    stab_chords, stab_times = [], []
    pad_chords, pad_times = [], []
    sections = []

    bar_index = 0
    for sec in arr:
        layers = sec["layers"]
        first = bar_index
        for b in range(sec["bars"]):
            g_bar = bar_index
            t0 = g_bar * bar
            chord = chords[prog[g_bar % len(prog)]]
            is_build = sec["section"] == "build"
            is_break = sec["section"] == "break"
            last_build_bar = is_build and b == sec["bars"] - 1
            build_u = (b / max(1, sec["bars"] - 1)) if is_build else 0.0

            def hits(name):
                return [(s, int(c) / 9.0) for s, c in enumerate(pat[name]) if c != "."]

            if layers.get("kick") and not last_build_bar:
                for s, v in hits("kick"):
                    mx.place("drums", kick, t0 + s * step, MIX["kick"] * v)
                    kick_times.append(t0 + s * step)
            if layers.get("clap"):
                for s, v in hits("clap"):
                    mx.place("clap", clap, t0 + s * step, MIX["clap"] * v, 0.08)
            hat_bus = "hats_break" if is_break else "drums"
            if layers.get("hat_off") and not last_build_bar:
                for s, v in hits("hat_off"):
                    mx.place(hat_bus, hat, t0 + s * step, MIX["hat"] * v * float(layers["hat_off"]), -0.25)
            if layers.get("hat_16"):
                for s, v in hits("hat_16"):
                    mx.place("drums", hat, t0 + s * step, MIX["hat"] * v, -0.25 if s % 2 == 0 else 0.25)
            if layers.get("ohat"):
                for s, v in hits("ohat"):
                    mx.place("drums", ohat, t0 + s * step, MIX["ohat"] * v, 0.2)
            if layers.get("shaker"):
                for s, v in hits("shaker"):
                    mx.place("drums", shaker, t0 + s * step, MIX["shaker"] * v, 0.45 if s % 2 else -0.45)
            if layers.get("pulse"):
                for s, v in hits("pulse"):
                    mx.place("pulse", pulse, t0 + s * step, MIX["pulse"] * v)
            if layers.get("stab") and not last_build_bar:
                for s, v in hits("stab"):
                    stab_chords.append((chord["stab"], v))
                    stab_times.append(t0 + s * step)

            # snare roll: quarter, eighth, sixteenth, thirty-second notes across the build
            if is_build:
                div = [4, 2, 1, 0.5][min(3, b)]
                count = int(round(16 / div))
                for k in range(count):
                    ts = k * div
                    if last_build_bar and ts >= 8:
                        break
                    level = 0.35 + 0.65 * ((b + ts / 16.0) / sec["bars"])
                    mx.place("snare", snare, t0 + ts * step, MIX["snare"] * level, 0.0)

            bass_name = layers.get("bass")
            if bass_name and not (is_build and b >= 2):
                key = bass_name
                if bass_name == "groove" and g_bar % 2 == 1:
                    key = "groove_alt"
                for s, off, v, length in track["bass"][key]:
                    fade = 1.0 - 0.5 * build_u
                    bass_notes.append((chord["bass"] + off, v * fade, length))
                    bass_times.append(t0 + s * step)

            lead = layers.get("lead")
            if lead:
                arp = chord["arp"]
                if lead == "double":
                    for k in range(32):
                        lead_notes.append((arp[k % 16], 0.85 if k % 2 == 0 else 0.7))
                        lead_times.append(t0 + k * step * 0.5)
                        lead_pans.append(0.25 * math.sin(k * 0.9))
                elif lead == "sparse":
                    for s in (0, 6, 12):
                        lead_notes.append((arp[s], 0.8))
                        lead_times.append(t0 + s * step)
                        lead_pans.append(0.3 if s == 6 else -0.2)
                else:
                    for s in range(16):
                        if last_build_bar and s >= 8:
                            break
                        accent = 1.0 if s % 4 == 0 else (0.82 if s % 2 == 0 else 0.7)
                        lead_notes.append((arp[s], accent))
                        lead_times.append(t0 + s * step)
                        lead_pans.append(0.25 * math.sin(s * 0.9 + g_bar))

            if layers.get("pad"):
                brightness = 0.55 if is_break else (1.0 + 0.9 * build_u if is_build else (1.5 if sec["section"] == "drop" else 1.0))
                pad_chords.append((chord["pad"], float(layers["pad"]), brightness))
                pad_times.append(t0)
            bar_index += 1
        sections.append((sec["section"], first, bar_index))

    # pitched voices
    y = render_plucks(lead_notes, PATCH["lead"])
    for i in range(len(lead_notes)):
        mx.place("lead", y[i], lead_times[i], MIX["lead"], lead_pans[i])
    if bass_notes:
        y = render_bass(bass_notes, PATCH["bass"], step)
        for i in range(len(bass_notes)):
            mx.place("bass", y[i], bass_times[i], MIX["bass"])
    if stab_chords:
        y = render_stabs(stab_chords, PATCH["stab"])
        for i in range(len(stab_chords)):
            mx.place("stab", y[i], stab_times[i], MIX["stab"], 0.3 if i % 2 else -0.3)
    if pad_chords:
        y = render_pads(pad_chords, PATCH["pad"], bar)
        for i in range(len(pad_chords)):
            mx.place("pad", y[i], pad_times[i], MIX["pad"])

    # section effects
    for name, first, end in sections:
        t_first, t_end = first * bar, end * bar
        if name == "build":
            mx.place("fx", make_riser(t_end - t_first), t_first, MIX["riser"])
            mx.place("fx", make_swell(bar * 0.5), t_end - bar * 0.5, MIX["swell"])
        if name == "drop" and first > 0 and sections[[s[1] for s in sections].index(first) - 1][0] == "build":
            mx.place("fx", impact, t_first, MIX["impact"])
        if name == "break":
            mx.place("fx", make_swell(bar), t_end - bar, MIX["swell"] * 0.7)

    # sidechain from the kick
    n = mx.n
    duck = np.zeros(n)
    tau = PATCH["sidechain_tau"]
    seg = np.exp(-np.arange(int(0.6 * SR)) / (tau * SR))
    for tk in kick_times:
        i = int(round(tk * SR))
        m = min(len(seg), n - i)
        duck[i:i + m] = np.maximum(duck[i:i + m], seg[:m])
    for name, depth in DUCK.items():
        if name in mx.buses and depth > 0:
            mx.buses[name] *= (1.0 - depth * duck)[:, None]

    b = mx.buses
    if "hats_break" in b:
        for ch in range(2):
            b["hats_break"][:, ch] = fft_filter(b["hats_break"][:, ch], lp(5000.0))
    d = PATCH["delay"]
    b["lead"] = ping_pong(b["lead"], d["steps"] * step, d["feedback"], d["mix"], d["repeats"])

    send = np.zeros((n, 2))
    for name, amount in SEND.items():
        if name in b:
            send += b[name] * amount
    wet = reverb(send, PATCH["reverb"])

    master = wet.copy()
    for x in b.values():
        master += x
    for ch in range(2):
        master[:, ch] = fft_filter(master[:, ch], hp(30.0))
    times, gains = [], []
    for name, first, end in sections:
        g = SECTION_GAIN[name]
        g0, g1 = g if isinstance(g, tuple) else (g, g)
        times += [first * bar + 0.03, end * bar - 0.03]
        gains += [g0, g1]
    master *= np.interp(np.arange(n) / SR, times, gains)[:, None]
    master = np.tanh(master * PATCH["master_drive"])
    gain = 10.0 ** ((TARGET_LUFS - lufs(master)) / 20.0)
    gain = min(gain, 0.891 / np.max(np.abs(master)))
    master *= gain
    stems = {k: v for k, v in b.items()}
    stems["reverb"] = wet
    return master, stems, dict(bpm=bpm, bars=total_bars, seconds=n / SR, sections=sections)


def lufs(x):
    """Integrated loudness (ITU-R BS.1770: K-weighting, 400 ms blocks, absolute and relative gates)."""
    n = len(x)
    w = 2 * PI * np.fft.rfftfreq(n, 1.0)
    z1, z2 = np.exp(-1j * w), np.exp(-2j * w)
    shelf = (1.53512485958697 - 2.69169618940638 * z1 + 1.19839281085285 * z2) / (1 - 1.69065929318241 * z1 + 0.73248077421585 * z2)
    rlb = (1.0 - 2.0 * z1 + z2) / (1 - 1.99004745483398 * z1 + 0.99007225036621 * z2)
    k = np.stack([np.fft.irfft(np.fft.rfft(x[:, ch]) * shelf * rlb, n) for ch in range(x.shape[1])], axis=1)
    block, hop = int(0.4 * SR), int(0.1 * SR)
    power = np.array([np.mean(k[i:i + block] ** 2, axis=0).sum() for i in range(0, n - block, hop)])
    loud = -0.691 + 10 * np.log10(np.maximum(power, 1e-12))
    keep = power[loud > -70.0]
    if len(keep) == 0:
        return -70.0
    rel = -0.691 + 10 * math.log10(keep.mean()) - 10.0
    keep = power[loud > max(rel, -70.0)]
    return -0.691 + 10 * math.log10(keep.mean())


def write_wav(path, x):
    r = rng(99)
    dither = (r.random(x.shape) - r.random(x.shape)) / 32768.0
    pcm = np.clip(x + dither, -1.0, 1.0)
    pcm = (pcm * 32767.0).astype("<i2")
    with wave.open(path, "wb") as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes(pcm.tobytes())


def band_report(x):
    mono = x.mean(axis=1)
    spec = np.abs(np.fft.rfft(mono * np.hanning(len(mono)))) ** 2
    f = np.fft.rfftfreq(len(mono), 1.0 / SR)
    bands = [("sub 30-80", 30, 80), ("bass 80-250", 80, 250), ("lowmid 250-800", 250, 800),
             ("mid 800-2.5k", 800, 2500), ("himid 2.5k-6k", 2500, 6000), ("high 6k-16k", 6000, 16000)]
    total = spec[(f >= 30) & (f < 16000)].sum()
    return [(name, 10.0 * math.log10(max(1e-20, spec[(f >= lo) & (f < hi)].sum() / total))) for name, lo, hi in bands]


def main():
    src = sys.argv[1]
    out = sys.argv[2] if len(sys.argv) > 2 else os.path.splitext(src)[0] + ".wav"
    track = json.load(open(src, encoding="utf-8"))
    master, stems, info = render(track)
    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
    write_wav(out, master)
    peak = float(np.max(np.abs(master)))
    rms = float(np.sqrt(np.mean(master ** 2)))
    corr = float(np.corrcoef(master[:, 0], master[:, 1])[0, 1])
    print("track %s  bpm %d  bars %d  %.1f s" % (track["id"], info["bpm"], info["bars"], info["seconds"]))
    print("lufs %.1f  peak %.2f dBFS  rms %.2f dBFS  crest %.1f dB  L/R correlation %.2f  dc %.5f" % (
        lufs(master), 20 * math.log10(peak), 20 * math.log10(rms), 20 * math.log10(peak / rms), corr, float(master.mean())))
    print("bands (dB of total): " + "  ".join("%s %.1f" % (n, v) for n, v in band_report(master)))
    phone = np.stack([fft_filter(master[:, ch], bp(280.0, 9000.0, 3)) for ch in range(2)], axis=1)
    print("phone-speaker band (280 Hz - 9 kHz) keeps %.1f dB of the mix RMS" % (
        20 * math.log10(np.sqrt(np.mean(phone ** 2)) / rms)))
    print("stems rms dBFS: " + "  ".join("%s %.1f" % (k, 20 * math.log10(max(1e-9, float(np.sqrt(np.mean(v ** 2)))))) for k, v in sorted(stems.items())))
    # Which stem owns which part of the spectrum (dB relative to the loudest stem-band cell).
    edges = [30, 80, 250, 800, 2500, 6000, 16000]
    table = {}
    for k, v in stems.items():
        mono = v.mean(axis=1)
        spec = np.abs(np.fft.rfft(mono)) ** 2
        f = np.fft.rfftfreq(len(mono), 1.0 / SR)
        table[k] = [spec[(f >= edges[i]) & (f < edges[i + 1])].sum() for i in range(6)]
    top = max(max(row) for row in table.values())
    print("stem x band (dB re loudest cell):   sub   bass lowmid    mid  himid   high")
    for k in sorted(table):
        print("  %-11s" % k + "".join("%7.1f" % (10 * math.log10(max(c, 1e-30) / top)) for c in table[k]))
    for name, first, end in info["sections"]:
        a, bb = int(first * 240.0 / info["bpm"] * SR), int(end * 240.0 / info["bpm"] * SR)
        seg = master[a:bb]
        print("  bars %2d-%2d %-6s rms %.1f dBFS" % (first + 1, end, name, 20 * math.log10(max(1e-9, float(np.sqrt(np.mean(seg ** 2)))))))
    print("wrote", out)


if __name__ == "__main__":
    main()

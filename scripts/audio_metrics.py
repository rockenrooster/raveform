"""Shared measurements for the M2 audio checks (prototype references and engine renders alike).

Loudness and bands use the prototype's own functions (design/tools/musicproto.py: lufs, band_report),
so both sides are measured by the same code. True peak is the 4x oversampled peak (ITU-R BS.1770-4
style, band-limited interpolation by FFT).
"""
import math
import os
import struct
import sys

import numpy as np

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "design", "tools"))
import musicproto as mp  # noqa: E402

SR = mp.SR
BAND_NAMES = ["sub", "bass", "lowmid", "mid", "himid", "high"]


def write_wav_float(path, x):
    """32-bit float stereo WAV (exact, for analysis)."""
    x = np.asarray(x, dtype="<f4")
    data = x.tobytes()
    with open(path, "wb") as f:
        f.write(b"RIFF" + struct.pack("<I", 36 + len(data)) + b"WAVE")
        f.write(b"fmt " + struct.pack("<IHHIIHH", 16, 3, x.shape[1], SR, SR * 4 * x.shape[1], 4 * x.shape[1], 32))
        f.write(b"data" + struct.pack("<I", len(data)))
        f.write(data)


def read_wav(path):
    """Returns (float64 array (n, channels), sample rate). PCM16, PCM24, PCM32 or float32."""
    with open(path, "rb") as f:
        raw = f.read()
    if raw[:4] != b"RIFF" or raw[8:12] != b"WAVE":
        raise ValueError(path + ": not a WAV file")
    pos = 12
    fmt = None
    while pos + 8 <= len(raw):
        cid, size = raw[pos:pos + 4], struct.unpack("<I", raw[pos + 4:pos + 8])[0]
        body = raw[pos + 8:pos + 8 + size]
        if cid == b"fmt ":
            fmt = struct.unpack("<HHIIHH", body[:16])
        elif cid == b"data":
            tag, ch, rate, _, _, bits = fmt
            if tag == 3 or (tag == 0xFFFE and bits == 32 and False):
                x = np.frombuffer(body, dtype="<f4").astype(np.float64)
            elif bits == 16:
                x = np.frombuffer(body, dtype="<i2").astype(np.float64) / 32768.0
            elif bits == 32:
                x = np.frombuffer(body, dtype="<i4").astype(np.float64) / 2147483648.0
            else:
                raise ValueError(path + ": unsupported WAV format %d/%d bits" % (tag, bits))
            return x.reshape(-1, ch), rate
        pos += 8 + size + (size & 1)
    raise ValueError(path + ": no data chunk")


def true_peak(x, factor=4):
    peak = 0.0
    for ch in range(x.shape[1]):
        s = x[:, ch]
        n = len(s)
        spec = np.fft.rfft(s)
        up = np.zeros(n * factor // 2 + 1, dtype=complex)
        up[:len(spec)] = spec
        y = np.fft.irfft(up, n * factor) * factor
        peak = max(peak, float(np.max(np.abs(y))), float(np.max(np.abs(s))))
    return peak


def db(v):
    return 20.0 * math.log10(max(v, 1e-12))


def section_levels(x, bpm, sections):
    out = []
    for name, first, end in sections:
        a, b = int(first * 240.0 / bpm * SR), int(end * 240.0 / bpm * SR)
        seg = x[a:b]
        out.append({"section": name, "first_bar": first, "end_bar": end,
                    "rms_db": db(float(np.sqrt(np.mean(seg ** 2)))) if len(seg) else -240.0})
    return out


def stem_bands(stems):
    edges = [30, 80, 250, 800, 2500, 6000, 16000]
    table = {}
    for k, v in stems.items():
        mono = v.mean(axis=1)
        spec = np.abs(np.fft.rfft(mono)) ** 2
        f = np.fft.rfftfreq(len(mono), 1.0 / SR)
        table[k] = [10 * math.log10(max(1e-30, float(spec[(f >= edges[i]) & (f < edges[i + 1])].sum()))) for i in range(6)]
    return table


def measure(x, bpm=None, sections=None):
    peak = float(np.max(np.abs(x)))
    rms = float(np.sqrt(np.mean(x ** 2)))
    phone = np.stack([mp.fft_filter(x[:, ch], mp.bp(280.0, 9000.0, 3)) for ch in range(x.shape[1])], axis=1)
    m = {
        "lufs": float(mp.lufs(x)),
        "peak_db": db(peak),
        "true_peak_db": db(true_peak(x)),
        "rms_db": db(rms),
        "crest_db": db(peak) - db(rms),
        "phone_db": db(float(np.sqrt(np.mean(phone ** 2)))) - db(rms),
        "bands_db": [float(v) for _, v in mp.band_report(x)],
        "lr_correlation": float(np.corrcoef(x[:, 0], x[:, 1])[0, 1]) if x.shape[1] == 2 else 1.0,
    }
    if bpm and sections:
        m["sections"] = section_levels(x, bpm, sections)
    return m

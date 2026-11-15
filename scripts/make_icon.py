"""Draw RAVEFORM's app icon from the design data (design/screens.md "App icon and name (M6)").

    python -I -B scripts/make_icon.py [--res android/app/src/main/res] [--out captures/m6]

The player's ship glyph (design/glyphs.json "ship"), nose up and to the right at 30 degrees, in P1 cyan with its white core
line, inside its halo of four cell arcs ("halo_4"), on the Ultraviolet palette's bg1-to-bg0 radial ground with the lattice's
lines at rest behind it, bulging a little under the ship. Colours, line levels and widths, the colour fields, the bloom (at
half strength) and the tone map come from design/look.json and design/palettes.json, drawn the way the engine draws them
(capsule coverage, glyph colour fields, scene + bloom + background, then the tone map; look.md sections 2, 3, 7 and 9).

Writes the adaptive icon's layers (background: ground and lattice; foreground: ship and halo with their bloom; monochrome:
ship and halo as an alpha mask for themed icons) at every density into the Android resources, the adaptive icon's XML, and
a 512 px square picture plus a round-masked review copy into --out. Reads design/, never writes under it.
"""
import argparse
import json
import math
import os
import sys

import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DENSITIES = {"mdpi": 108, "hdpi": 162, "xhdpi": 216, "xxhdpi": 324, "xxxhdpi": 432}  # the 108 dp layer in pixels
SS = 4                      # supersampling (the picture is drawn at SS x and averaged after the tone map)
SHIP_SPAN = 0.44            # screens.md: the ship's extent 44% of the icon (its span, nose to tail: the halo then fits every mask)
HEADING_DEG = 30.0          # nose up and to the right
REF_PX_PER_WU = 1080.0 / 720.0  # the game at zoom 1: 720 wu of height on 1080 lines (look.md units: ref px = height / 1080)
GRID_WU = 20.0              # the lattice's spacing on the reference (high) tier, src/game/tiers.cpp
BULGE_SIGMA_WU = 26.0       # the bulge under the ship: a radial push, at most about 4 wu (chosen: "a little")
BULGE_AMOUNT = 0.25
BLOOM_SHARE = 0.5           # screens.md: the bloom of look.md section 7 at half strength


def load(path):
    with open(os.path.join(ROOT, path), "r", encoding="utf-8") as f:
        return json.load(f)


def lin(c):
    return np.array(c["linear"], dtype=np.float64)


def tonemap(rgb, look):
    """look.md 3: extended Reinhard on the max channel (hue preserving), very hot values bleeding toward white."""
    tm_cfg = look["toneMap"]
    wp = tm_cfg["whitePoint"]
    m = np.max(rgb, axis=-1, keepdims=True)
    m_safe = np.maximum(m, 1e-8)
    tm = np.minimum(1.0, m * (1.0 + m / (wp * wp)) / (1.0 + m))
    out = rgb * (tm / m_safe)
    e0, e1 = tm_cfg["bleedStart"], tm_cfg["bleedEnd"]
    x = np.clip((m - e0) / (e1 - e0), 0.0, 1.0)
    t = x * x * (3.0 - 2.0 * x) * tm_cfg["bleedAmount"]
    return out * (1.0 - t) + tm * t


def srgb(x):
    x = np.clip(x, 0.0, 1.0)
    return np.where(x <= 0.0031308, 12.92 * x, 1.055 * np.power(x, 1.0 / 2.4) - 0.055)


def blur(img, sigma):
    """A Gaussian blur through the FFT, padded by three sigmas (zero outside: light leaves the icon)."""
    pad = int(math.ceil(3.0 * sigma)) + 1
    h, w = img.shape[:2]
    H, W = h + 2 * pad, w + 2 * pad
    fy = np.fft.fftfreq(H)[:, None]
    fx = np.fft.rfftfreq(W)[None, :]
    g = np.exp(-2.0 * (math.pi ** 2) * (sigma ** 2) * (fx * fx + fy * fy))
    out = np.empty_like(img)
    for c in range(img.shape[2]):
        a = np.zeros((H, W))
        a[pad:pad + h, pad:pad + w] = img[:, :, c]
        out[:, :, c] = np.fft.irfft2(np.fft.rfft2(a) * g, s=(H, W))[pad:pad + h, pad:pad + w]
    return out


def bloom(scene, look, ref_px):
    b = look["bloom"]
    acc = np.zeros_like(scene)
    for s, wgt in zip(b["sigmasRefPx"], b["weights"]):
        acc += wgt * blur(scene, s * ref_px)
    return scene + BLOOM_SHARE * b["strength"] * acc


class Layer:
    """Capsules of one shape (look.md 9): coverage clamp(r + 0.5 - d, 0, 1), colour along the segment. Inside one shape the
    coverage is the union (the nearest segment's colour), so a joint is not lit twice; shapes add."""

    def __init__(self, n):
        self.cover = np.zeros((n, n))
        self.col = np.zeros((n, n, 3))

    def seg(self, p0, p1, r, c0, c1):
        n = self.cover.shape[0]
        x0 = max(int(math.floor(min(p0[0], p1[0]) - r - 2)), 0)
        x1 = min(int(math.ceil(max(p0[0], p1[0]) + r + 2)), n)
        y0 = max(int(math.floor(min(p0[1], p1[1]) - r - 2)), 0)
        y1 = min(int(math.ceil(max(p0[1], p1[1]) + r + 2)), n)
        if x0 >= x1 or y0 >= y1:
            return
        ys, xs = np.mgrid[y0:y1, x0:x1].astype(np.float64)
        xs += 0.5
        ys += 0.5
        d = np.array(p1) - np.array(p0)
        l2 = float(d @ d)
        t = np.zeros_like(xs) if l2 < 1e-12 else np.clip(((xs - p0[0]) * d[0] + (ys - p0[1]) * d[1]) / l2, 0.0, 1.0)
        dx = xs - (p0[0] + t * d[0])
        dy = ys - (p0[1] + t * d[1])
        cov = np.clip(r + 0.5 - np.sqrt(dx * dx + dy * dy), 0.0, 1.0)
        col = c0[None, None, :] * (1.0 - t[..., None]) + c1[None, None, :] * t[..., None]
        cur = self.cover[y0:y1, x0:x1]
        take = cov > cur
        cur[take] = cov[take]
        self.col[y0:y1, x0:x1][take] = col[take]

    def light(self):
        return self.col * self.cover[..., None]


def stops3(t, a, b, c):
    t = min(max(t, 0.0), 1.0) * 2.0
    return a + (b - a) * t if t < 1.0 else b + (c - b) * (t - 1.0)


def ring4(t, s0, s1, s2, s3):
    t = (t % 1.0) * 4.0
    u = t - math.floor(t)
    s = [s0, s1, s2, s3, s0]
    k = int(math.floor(t)) % 4
    return s[k] + (s[k + 1] - s[k]) * u


def render(n, look, glyphs, palettes):
    """The scene at n x n pixels: (ground light, lattice light, ship-and-halo light), linear HDR, unbloomed; and the ref px."""
    white = np.ones(3)
    consts = palettes["constants"]
    p1 = consts["players"][0]
    player, deep, light = lin(p1), lin(p1["deep"]), lin(p1["light"])
    uv = next(p for p in palettes["palettes"] if p["id"] == "ultraviolet")
    ship = next(g for g in glyphs["glyphs"] if g["id"] == "ship")
    halo = next(g for g in glyphs["glyphs"] if g["id"] == "halo_4")
    lines = look["lines"]

    unit_px = SHIP_SPAN * n / (2.0 * ship["extent"])  # pixels per glyph unit
    wu_px = unit_px / (14.0 * ship["size"])           # a glyph unit is 14 wu x the glyph's size (look.md 4)
    ref_px = wu_px / REF_PX_PER_WU
    c = n * 0.5
    ang = math.radians(HEADING_DEG)
    ca, sa = math.cos(ang), math.sin(ang)

    def place(x, y, rot=True):  # glyph units, y up -> image pixels, y down
        if rot:
            x, y = x * ca - y * sa, x * sa + y * ca
        return (c + x * unit_px, c - y * unit_px)

    # Ground: bg1 at the centre to bg0 at the edge, at the gradient's level, over 0.8 heights with the falloff exponent.
    g = look["background"]["gradient"]
    ys, xs = np.mgrid[0:n, 0:n].astype(np.float64)
    d = np.sqrt((xs + 0.5 - c) ** 2 + (ys + 0.5 - c) ** 2) / (g["radiusScreenHeights"] * n)
    f = np.clip(d, 0.0, 1.0) ** g["power"]
    ground = (lin(uv["bg1"])[None, None, :] * (1.0 - f[..., None]) + lin(uv["bg0"])[None, None, :] * f[..., None]) * g["level"]

    # The lattice at rest (look.md 2, 3): grid lines GRID_WU apart through the ship, pushed out a little under it; the colour
    # field from the ship out over the tint radius: 55% grid + 45% player -> grid -> 70% grid + 30% the third enemy hue.
    lat = Layer(n)
    grid_i, grid_w = lines["gridRest"]
    grid = lin(uv["grid"])
    cand = uv["enemyCandidates"]
    third = lin(cand[uv["soloEnemies"][2]])
    near_mix = look["grid"]["tintNearPlayerMix"]
    far_mix = look["grid"]["tintFarThirdHueMix"]
    tint_r = look["grid"]["tintRadiusWu"]
    half_wu = (n * 0.5) / wu_px + GRID_WU

    def lattice_col(xw, yw):
        r = math.hypot(xw, yw) / tint_r
        return stops3(r, grid * (1.0 - near_mix) + player * near_mix, grid, grid * (1.0 - far_mix) + third * far_mix) * grid_i

    def bulge(xw, yw):
        r = math.hypot(xw, yw)
        k = 1.0 + BULGE_AMOUNT * math.exp(-(r * r) / (2.0 * BULGE_SIGMA_WU ** 2))
        return xw * k, yw * k

    k_lines = int(math.ceil(half_wu / GRID_WU))
    steps = 160
    for axis in (0, 1):
        for k in range(-k_lines, k_lines + 1):
            fixed = k * GRID_WU
            prev = None
            for s in range(steps + 1):
                v = -half_wu + 2.0 * half_wu * s / steps
                xw, yw = (fixed, v) if axis == 0 else (v, fixed)
                bx, by = bulge(xw, yw)
                pt = (c + bx * wu_px, c - by * wu_px)
                col = lattice_col(xw, yw)
                if prev is not None:
                    lat.seg(prev[0], pt, 0.5 * grid_w * ref_px, prev[1], col)
                prev = (pt, col)

    # The ship: the hull in the player's field (white -> player -> deep over 1.3 glyph units), its white copy, the core mark
    # white at the hull's level x 1.1; then the halo's four arcs (player, white, player, light around the centre).
    fg = np.zeros((n, n, 3))
    hull_i, hull_w = lines["playerHull"]
    wc_i, wc_w = lines["playerWhiteCopy"]
    core_i, core_w = hull_i * look["enemy"]["coreIntensityOfHull"], look["enemy"]["coreWidth"]
    hull, copy, core = Layer(n), Layer(n), Layer(n)
    for x0, y0, x1, y1, layer in ship["segments"]:
        a, b = place(x0, y0), place(x1, y1)
        if layer == 0:
            hull.seg(a, b, 0.5 * hull_w * ref_px, stops3(math.hypot(x0, y0) / 1.3, white, player, deep) * hull_i,
                     stops3(math.hypot(x1, y1) / 1.3, white, player, deep) * hull_i)
            copy.seg(a, b, 0.5 * wc_w * ref_px, white * wc_i, white * wc_i)
        elif layer == 2:
            core.seg(a, b, 0.5 * core_w * ref_px, white * core_i, white * core_i)
    halo_i, halo_w = lines["playerHalo"]
    arcs = Layer(n)
    for x0, y0, x1, y1, _layer in halo["segments"]:
        # (the engine's mesh is y down: the ring's angle is measured there)
        c0 = ring4(math.atan2(-y0, x0) / (2.0 * math.pi), player, white, player, light) * halo_i
        c1 = ring4(math.atan2(-y1, x1) / (2.0 * math.pi), player, white, player, light) * halo_i
        arcs.seg(place(x0, y0, rot=False), place(x1, y1, rot=False), 0.5 * halo_w * ref_px, c0, c1)
    fg = hull.light() + copy.light() + core.light() + arcs.light()
    return ground, lat.light(), fg, ref_px


def down(img, ss):
    h, w = img.shape[0] // ss, img.shape[1] // ss
    return img.reshape(h, ss, w, ss, -1).mean(axis=(1, 3))


def to8(x):
    return np.clip(np.round(x * 255.0), 0, 255).astype(np.uint8)


def draw(size, look, glyphs, palettes):
    """Returns (background RGB, foreground RGBA, monochrome RGBA, the whole icon RGB), each size x size, 8-bit."""
    n = size * SS
    ground, lattice, ship, ref_px = render(n, look, glyphs, palettes)
    exposure = look["exposure"]
    # Background layer: the lattice and its bloom, then the ground (added after the bloom, not bloomed), tone mapped.
    bg_lin = tonemap((bloom(lattice, look, ref_px) + ground) * exposure, look)
    # Foreground layer: the ship and halo with their bloom, tone mapped alone; alpha = the brightest channel (the light it
    # adds over the dark ground), colour = what it adds / alpha.
    fg_tm = tonemap(bloom(ship, look, ref_px) * exposure, look)
    # The whole icon: one scene, one bloom, one tone map.
    all_lin = tonemap((bloom(lattice + ship, look, ref_px) + ground) * exposure, look)
    bg = to8(srgb(down(bg_lin, SS)))
    allc = to8(srgb(down(all_lin, SS)))
    fg_d = down(fg_tm, SS)
    a = np.clip(np.max(fg_d, axis=-1, keepdims=True), 0.0, 1.0)
    rgb = np.where(a > 1e-6, fg_d / np.maximum(a, 1e-6), 0.0)
    fg = np.concatenate([to8(srgb(rgb)), to8(a)], axis=-1)
    # Monochrome (themed icons): the ship and halo as an alpha mask, white; the faint wide bloom left out (under 4%).
    lum = np.clip(np.max(fg_d, axis=-1, keepdims=True), 0.0, 1.0)
    lum = np.clip((lum - 0.04) / 0.96, 0.0, 1.0)
    mono = np.concatenate([np.full_like(to8(rgb), 255), to8(lum)], axis=-1)
    return bg, fg, mono, allc


ADAPTIVE_XML = """<?xml version="1.0" encoding="utf-8"?>
<!-- RAVEFORM's icon (design/screens.md "App icon and name (M6)"): drawn by scripts/make_icon.py from design/glyphs.json and
     design/palettes.json. Do not edit the layers by hand; run the script again. -->
<adaptive-icon xmlns:android="http://schemas.android.com/apk/res/android">
    <background android:drawable="@mipmap/ic_launcher_background" />
    <foreground android:drawable="@mipmap/ic_launcher_foreground" />
    <monochrome android:drawable="@mipmap/ic_launcher_monochrome" />
</adaptive-icon>
"""


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--res", default=os.path.join(ROOT, "android", "app", "src", "main", "res"))
    ap.add_argument("--out", default=os.path.join(ROOT, "captures", "m6"))
    # docs/IOS.md: the iOS app icon, one 1024 px square without alpha (iOS rounds its corners): the whole icon as the adaptive
    # one's visible 72 dp of 108 (rendered at 1536 and its middle two thirds kept).
    ap.add_argument("--ios", default=os.path.join(ROOT, "ios", "Assets.xcassets", "AppIcon.appiconset"))
    ap.add_argument("--ios-only", action="store_true", help="write only the iOS icon")
    args = ap.parse_args()
    look = load("design/look.json")
    glyphs = load("design/glyphs.json")
    palettes = load("design/palettes.json")
    design = os.path.realpath(os.path.join(ROOT, "design"))
    for path in (args.res, args.out, args.ios):
        if os.path.realpath(path).startswith(design):
            print("make_icon: never writes under design/", file=sys.stderr)
            return 2
    os.makedirs(args.ios, exist_ok=True)
    # (rendered at 1024 and its middle two thirds scaled up to 1024: at 1536 the render needs some 20 GB and most of an hour;
    # the home screen shows the icon at 180 px at most)
    _, _, _, big = draw(1024, look, glyphs, palettes)
    cut = int(round(1024 * 18.0 / 108.0))
    ios_icon = Image.fromarray(big, "RGB").crop((cut, cut, 1024 - cut, 1024 - cut)).resize((1024, 1024), Image.LANCZOS)
    ios_icon.save(os.path.join(args.ios, "icon_1024.png"), optimize=True)
    print("make_icon: %s" % os.path.join(args.ios, "icon_1024.png"))
    if args.ios_only:
        return 0
    for dens, px in DENSITIES.items():
        bg, fg, mono, _ = draw(px, look, glyphs, palettes)
        d = os.path.join(args.res, "mipmap-" + dens)
        os.makedirs(d, exist_ok=True)
        Image.fromarray(bg, "RGB").save(os.path.join(d, "ic_launcher_background.png"), optimize=True)
        Image.fromarray(fg, "RGBA").save(os.path.join(d, "ic_launcher_foreground.png"), optimize=True)
        Image.fromarray(mono, "RGBA").save(os.path.join(d, "ic_launcher_monochrome.png"), optimize=True)
        print("make_icon: mipmap-%s (%d px)" % (dens, px))
    any_dir = os.path.join(args.res, "mipmap-anydpi-v26")
    os.makedirs(any_dir, exist_ok=True)
    with open(os.path.join(any_dir, "ic_launcher.xml"), "w", encoding="utf-8", newline="\n") as f:
        f.write(ADAPTIVE_XML)
    # The 512 px picture (the whole square; the store and the launchers mask it), a round-masked copy (the most cut-down
    # mask: the 72 dp of 108 a launcher shows, in a circle) and the monochrome layer on grey, for review.
    os.makedirs(args.out, exist_ok=True)
    bg, fg, mono, allc = draw(512, look, glyphs, palettes)
    Image.fromarray(allc, "RGB").save(os.path.join(args.out, "icon_512.png"), optimize=True)
    crop = int(round(512 * 18.0 / 108.0))
    shown = Image.fromarray(allc, "RGB").crop((crop, crop, 512 - crop, 512 - crop)).resize((512, 512), Image.LANCZOS)
    yy, xx = np.mgrid[0:512, 0:512]
    circle = (np.sqrt((xx + 0.5 - 256) ** 2 + (yy + 0.5 - 256) ** 2) <= 256).astype(np.uint8) * 255
    round_img = Image.fromarray(np.concatenate([np.asarray(shown), circle[..., None]], axis=-1), "RGBA")
    round_img.save(os.path.join(args.out, "icon_512_round.png"), optimize=True)
    grey = np.full((512, 512, 3), 70, np.uint8)
    a = mono[..., 3:4].astype(np.float64) / 255.0
    themed = (grey * (1.0 - a) + np.array([214, 228, 255]) * a).astype(np.uint8)
    Image.fromarray(themed, "RGB").save(os.path.join(args.out, "icon_512_monochrome.png"), optimize=True)
    print("make_icon: %s" % os.path.join(args.out, "icon_512.png"))
    return 0


if __name__ == "__main__":
    sys.exit(main())

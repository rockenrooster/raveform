"""RAVEFORM look-development helpers (creative tooling, not game code).

Offline approximation of the in-game renderer so colours, line weights, glow and glyph shapes can be
judged in still images before the engine exists: an HDR float canvas of additive capsule lines with
optional colour gradients, soft glow sprites, faint glass fills, a multi-scale bloom, the game's tone
map, and OKLCH colour utilities. Numbers proven here are copied into design/look.md and the JSON data
for the engine.

Requires: numpy, Pillow.
"""
import math

import numpy as np
from PIL import Image, ImageDraw

SS = 3  # supersampling factor for anti-aliased masks


# --------------------------------------------------------------------------- colour (OKLCH)

def oklab_to_linear(L, a, b):
    l_ = L + 0.3963377774 * a + 0.2158037573 * b
    m_ = L - 0.1055613458 * a - 0.0638541728 * b
    s_ = L - 0.0894841775 * a - 1.2914855480 * b
    l, m, s = l_ ** 3, m_ ** 3, s_ ** 3
    return np.array([
        4.0767416621 * l - 3.3077115913 * m + 0.2309699292 * s,
        -1.2684380046 * l + 2.6097574011 * m - 0.3413193965 * s,
        -0.0041960863 * l - 0.7034186147 * m + 1.7076147010 * s,
    ], dtype=np.float64)


def oklch_raw(L, C, h_deg):
    hr = math.radians(h_deg)
    return oklab_to_linear(L, C * math.cos(hr), C * math.sin(hr))


def in_gamut(rgb, eps=1e-4):
    return bool(np.all(rgb >= -eps) and np.all(rgb <= 1.0 + eps))


def max_chroma(L, h_deg):
    """Largest chroma that stays inside sRGB for this lightness and hue."""
    lo, hi = 0.0, 0.5
    for _ in range(40):
        mid = (lo + hi) / 2
        if in_gamut(oklch_raw(L, mid, h_deg)):
            lo = mid
        else:
            hi = mid
    return lo


def oklch(L, h_deg, c=None, cf=None):
    """Linear sRGB (0..1) for an OKLCH colour. Give absolute chroma `c`, or `cf` as a fraction of the
    gamut maximum (default 1.0). Returns (rgb, chroma_used)."""
    cmax = max_chroma(L, h_deg)
    chroma = min(c, cmax) if c is not None else cmax * (1.0 if cf is None else cf)
    return np.clip(oklch_raw(L, chroma, h_deg), 0.0, 1.0), chroma


def neon(h_deg, L=0.80, cf=0.96):
    """A saturated neon colour at this hue, as a float32 linear RGB array."""
    return oklch(L, h_deg, cf=cf)[0].astype(np.float32)


def srgb_encode(x):
    x = np.clip(x, 0.0, 1.0)
    return np.where(x <= 0.0031308, 12.92 * x, 1.055 * np.power(x, 1 / 2.4) - 0.055)


def hexstr(linear_rgb):
    v = np.round(srgb_encode(np.asarray(linear_rgb, dtype=np.float64)) * 255).astype(int)
    return "#%02x%02x%02x" % (v[0], v[1], v[2])


def hue_distance(a, b):
    d = abs(a - b) % 360.0
    return min(d, 360.0 - d)


# --------------------------------------------------------------------------- tone map (the game's)

TONEMAP_WHITE = 6.0        # scene value that maps to display white
TONEMAP_BLEED_START = 1.0  # peak channel value where colour starts bleeding toward white
TONEMAP_BLEED_END = 8.0
TONEMAP_BLEED_AMOUNT = 0.7


def tonemap(c):
    """Hue-preserving: compress the brightest channel with an extended Reinhard curve, scale RGB by
    the same factor, then bleed very hot values toward white so cores go white and halos keep colour."""
    c = np.maximum(c, 0.0)
    m = c.max(axis=2, keepdims=True)
    tm = m * (1.0 + m / (TONEMAP_WHITE * TONEMAP_WHITE)) / (1.0 + m)
    tm = np.minimum(tm, 1.0)
    base = c * (tm / np.maximum(m, 1e-6))
    t = np.clip((m - TONEMAP_BLEED_START) / (TONEMAP_BLEED_END - TONEMAP_BLEED_START), 0.0, 1.0)
    white = t * t * (3.0 - 2.0 * t) * TONEMAP_BLEED_AMOUNT
    return base * (1.0 - white) + tm * white


# --------------------------------------------------------------------------- font

def load_font(path):
    glyphs = {}
    for line in open(path, encoding="utf-8"):
        line = line.rstrip("\n")
        if not line or line.startswith("//"):
            continue
        ch, _, rest = line.partition(" ")
        strokes = []
        for s in rest.split("|"):
            pts = [(int(p[0]), int(p[1])) for p in s.split()]
            if pts:
                strokes.append(pts)
        glyphs[ch] = strokes
    return glyphs


def text_segments(font, s, x, y, cap_height):
    unit = cap_height / 6.0
    segs = []
    cx = x
    for ch in s:
        for pts in font.get(ch.upper(), []):
            px = [(cx + p[0] * unit, y + (6 - p[1]) * unit) for p in pts]
            if len(px) == 1:
                segs.append((px[0][0], px[0][1], px[0][0], px[0][1]))
            else:
                for i in range(len(px) - 1):
                    segs.append((px[i][0], px[i][1], px[i + 1][0], px[i + 1][1]))
        cx += 6 * unit
    return segs, cx


# --------------------------------------------------------------------------- shapes (unit space)

def polygon(n, r=1.0, rot=0.0):
    pts = [(r * math.cos(rot + 2 * math.pi * i / n), r * math.sin(rot + 2 * math.pi * i / n)) for i in range(n)]
    return [(pts[i][0], pts[i][1], pts[(i + 1) % n][0], pts[(i + 1) % n][1]) for i in range(n)]


def star(n, r_outer=1.0, r_inner=0.45, rot=0.0):
    pts = []
    for i in range(2 * n):
        r = r_outer if i % 2 == 0 else r_inner
        a = rot + math.pi * i / n
        pts.append((r * math.cos(a), r * math.sin(a)))
    return [(pts[i][0], pts[i][1], pts[(i + 1) % len(pts)][0], pts[(i + 1) % len(pts)][1]) for i in range(len(pts))]


def arc(r, a0, a1, n=8):
    pts = [(r * math.cos(a0 + (a1 - a0) * i / n), r * math.sin(a0 + (a1 - a0) * i / n)) for i in range(n + 1)]
    return [(pts[i][0], pts[i][1], pts[i + 1][0], pts[i + 1][1]) for i in range(n)]


def polyline(points, closed=False):
    segs = [(points[i][0], points[i][1], points[i + 1][0], points[i + 1][1]) for i in range(len(points) - 1)]
    if closed and len(points) > 2:
        segs.append((points[-1][0], points[-1][1], points[0][0], points[0][1]))
    return segs


def transform(segs, x=0.0, y=0.0, scale=1.0, rot=0.0):
    c, s = math.cos(rot), math.sin(rot)
    out = []
    for seg in segs:
        x0, y0, x1, y1 = seg[:4]
        out.append((x + scale * (x0 * c - y0 * s), y + scale * (x0 * s + y0 * c),
                    x + scale * (x1 * c - y1 * s), y + scale * (x1 * s + y1 * c)))
    return out


def transform_points(points, x=0.0, y=0.0, scale=1.0, rot=0.0):
    c, s = math.cos(rot), math.sin(rot)
    return [(x + scale * (px * c - py * s), y + scale * (px * s + py * c)) for px, py in points]


# --------------------------------------------------------------------------- colour fields
# A field maps pixel coordinate arrays to linear RGB. In the engine a field is two or three colours
# stored on the instance and interpolated from the vertex's position inside the shape.

def f_solid(colour):
    c = np.asarray(colour, np.float32)
    return lambda xx, yy: np.broadcast_to(c, xx.shape + (3,))


def f_linear(x0, y0, x1, y1, colours):
    """Gradient along the line (x0,y0) -> (x1,y1) through two or more evenly spaced colours."""
    cols = np.asarray(colours, np.float32)
    dx, dy = x1 - x0, y1 - y0
    l2 = dx * dx + dy * dy or 1.0

    def f(xx, yy):
        t = np.clip(((xx - x0) * dx + (yy - y0) * dy) / l2, 0.0, 1.0) * (len(cols) - 1)
        i = np.minimum(t.astype(np.int32), len(cols) - 2)
        u = (t - i)[..., None]
        return cols[i] * (1.0 - u) + cols[i + 1] * u
    return f


def f_radial(cx, cy, radius, colours, power=1.0):
    """Gradient from the centre outward through two or more evenly spaced colours."""
    cols = np.asarray(colours, np.float32)

    def f(xx, yy):
        t = np.clip(np.hypot(xx - cx, yy - cy) / radius, 0.0, 1.0) ** power * (len(cols) - 1)
        i = np.minimum(t.astype(np.int32), len(cols) - 2)
        u = (t - i)[..., None]
        return cols[i] * (1.0 - u) + cols[i + 1] * u
    return f


def f_angular(cx, cy, colours, rot=0.0):
    """Colours around the centre, cyclic: a colour wheel."""
    cols = np.asarray(list(colours) + [colours[0]], np.float32)
    n = len(colours)

    def f(xx, yy):
        t = ((np.arctan2(yy - cy, xx - cx) - rot) / (2 * math.pi)) % 1.0 * n
        i = np.minimum(t.astype(np.int32), n - 1)
        u = (t - i)[..., None]
        return cols[i] * (1.0 - u) + cols[i + 1] * u
    return f


def f_fade(field, cx, cy, radius, power=1.5):
    """Multiply a field by a radial falloff to zero: flares and tapering rays."""
    def f(xx, yy):
        k = np.clip(1.0 - np.hypot(xx - cx, yy - cy) / radius, 0.0, 1.0) ** power
        return field(xx, yy) * k[..., None]
    return f


# --------------------------------------------------------------------------- canvas

class Canvas:
    """Additive HDR canvas in linear light. Coordinates are pixels, y down."""

    def __init__(self, w, h):
        self.w, self.h = w, h
        self.hdr = np.zeros((h, w, 3), np.float32)   # blooms
        self.bg = np.zeros((h, w, 3), np.float32)    # added in the composite, does not bloom
        self.post = []                                # (mask, x0, y0, colour) drawn after tone mapping

    # ---- masks, limited to the bounding box of what is drawn

    def _box(self, xs, ys, pad):
        x0 = max(0, int(math.floor(min(xs) - pad)))
        y0 = max(0, int(math.floor(min(ys) - pad)))
        x1 = min(self.w, int(math.ceil(max(xs) + pad)) + 1)
        y1 = min(self.h, int(math.ceil(max(ys) + pad)) + 1)
        return (x0, y0, x1, y1) if x1 > x0 and y1 > y0 else None

    def _mask(self, segs, width):
        box = self._box([v for s in segs for v in (s[0], s[2])], [v for s in segs for v in (s[1], s[3])], width / 2 + 2)
        if box is None:
            return None
        x0, y0, x1, y1 = box
        m = Image.new("L", ((x1 - x0) * SS, (y1 - y0) * SS), 0)
        d = ImageDraw.Draw(m)
        wpx = max(1, int(round(width * SS)))
        r = wpx / 2.0
        for seg in segs:
            ax, ay, bx, by = (seg[0] - x0) * SS, (seg[1] - y0) * SS, (seg[2] - x0) * SS, (seg[3] - y0) * SS
            if abs(ax - bx) > 1e-6 or abs(ay - by) > 1e-6:
                d.line([(ax, ay), (bx, by)], fill=255, width=wpx)
            d.ellipse([ax - r, ay - r, ax + r, ay + r], fill=255)
            d.ellipse([bx - r, by - r, bx + r, by + r], fill=255)
        a = np.asarray(m.resize((x1 - x0, y1 - y0), Image.BOX), np.float32) / 255.0
        return a, x0, y0

    def _poly_mask(self, points):
        box = self._box([p[0] for p in points], [p[1] for p in points], 2)
        if box is None:
            return None
        x0, y0, x1, y1 = box
        m = Image.new("L", ((x1 - x0) * SS, (y1 - y0) * SS), 0)
        ImageDraw.Draw(m).polygon([((p[0] - x0) * SS, (p[1] - y0) * SS) for p in points], fill=255)
        a = np.asarray(m.resize((x1 - x0, y1 - y0), Image.BOX), np.float32) / 255.0
        return a, x0, y0

    def _add(self, res, colour, intensity):
        if res is None:
            return
        a, x0, y0 = res
        h, w = a.shape
        if callable(colour):
            yy, xx = np.mgrid[y0:y0 + h, x0:x0 + w].astype(np.float32)
            self.hdr[y0:y0 + h, x0:x0 + w] += a[..., None] * colour(xx, yy) * intensity
        else:
            self.hdr[y0:y0 + h, x0:x0 + w] += a[..., None] * (np.asarray(colour, np.float32) * intensity)

    # ---- drawing. `colour` is a linear RGB triple or a field from the f_* functions above.

    def lines(self, segs, colour, intensity=1.0, width=1.6):
        segs = list(segs)
        if segs:
            self._add(self._mask(segs, width), colour, intensity)

    def fill(self, points, colour, intensity=0.1):
        """Faint additive fill of a closed polygon: the glass inside frames and special shapes."""
        self._add(self._poly_mask(points), colour, intensity)

    def discs(self, items, colour, intensity=1.0):
        """Filled circles (pressed buttons, thumb pads). items: iterable of (x, y, radius)."""
        for x, y, r in items:
            n = max(16, int(r))
            self.fill([(x + r * math.cos(2 * math.pi * k / n), y + r * math.sin(2 * math.pi * k / n)) for k in range(n)], colour, intensity)

    def glow(self, x, y, radius, colour, intensity=1.0):
        """Soft round glow sprite: Gaussian falling to about 5% at `radius`."""
        box = self._box([x], [y], radius * 1.25)
        if box is None:
            return
        x0, y0, x1, y1 = box
        yy, xx = np.mgrid[y0:y1, x0:x1].astype(np.float32)
        sigma = radius / 2.45
        g = np.exp(-((xx - x) ** 2 + (yy - y) ** 2) / (2 * sigma * sigma))
        self.hdr[y0:y1, x0:x1] += g[..., None] * (np.asarray(colour, np.float32) * intensity)

    def flare(self, x, y, length, colour, intensity=2.0, width=1.4, arms=4, rot=0.0):
        """Star streaks that taper to nothing: lens flares on pickups and special objects."""
        segs = []
        for k in range(arms):
            a = rot + math.pi * k / arms
            segs.append((x - math.cos(a) * length, y - math.sin(a) * length, x + math.cos(a) * length, y + math.sin(a) * length))
        self.lines(segs, f_fade(f_solid(colour), x, y, length, 1.6), intensity, width)

    def threat(self, segs, colour, width=2.2, rim=1.6):
        """Threat-layer draw: after tone mapping, alpha blended, with a dark rim."""
        segs = list(segs)
        if not segs:
            return
        for w_, col in ((width + 2 * rim, np.zeros(3, np.float32)), (width, np.asarray(colour, np.float32))):
            res = self._mask(segs, w_)
            if res is not None:
                self.post.append((res[0], res[1], res[2], col))

    def text(self, font, s, x, y, cap_height, colour, intensity=1.0, width=None):
        unit = cap_height / 6.0
        width = width if width is not None else max(1.2, 0.55 * unit)
        segs, end = text_segments(font, s, x, y, cap_height)
        self.lines(segs, colour, intensity, width)
        return end

    # ---- background (does not bloom)

    def radial_bg(self, edge, centre, cx=0.5, cy=0.5, radius=0.8, power=1.5):
        yy, xx = np.mgrid[0:self.h, 0:self.w].astype(np.float32)
        d = np.sqrt(((xx / self.w - cx) * self.w / self.h) ** 2 + (yy / self.h - cy) ** 2) / radius
        t = (np.clip(1.0 - d, 0.0, 1.0) ** power)[..., None]
        self.bg = np.asarray(edge, np.float32)[None, None, :] * (1 - t) + np.asarray(centre, np.float32)[None, None, :] * t

    def nebula(self, blobs):
        """Soft colour clouds behind everything. blobs: iterable of (x, y, radius, colour, intensity)."""
        for x, y, radius, colour, intensity in blobs:
            box = self._box([x], [y], radius * 1.3)
            if box is None:
                continue
            x0, y0, x1, y1 = box
            yy, xx = np.mgrid[y0:y1, x0:x1].astype(np.float32)
            sigma = radius / 2.2
            g = np.exp(-((xx - x) ** 2 + (yy - y) ** 2) / (2 * sigma * sigma))
            self.bg[y0:y1, x0:x1] += g[..., None] * (np.asarray(colour, np.float32) * intensity)

    # ---- composite

    def render(self, bloom=0.9, exposure=1.0,
               sigmas=(1.5, 3.0, 6.0, 12.0, 24.0, 48.0),
               weights=(0.26, 0.22, 0.18, 0.14, 0.11, 0.09)):
        pad = 128
        p = np.pad(self.hdr, ((pad, pad), (pad, pad), (0, 0)), mode="constant")
        H, W = p.shape[:2]
        fy = np.fft.fftfreq(H)[:, None]
        fx = np.fft.rfftfreq(W)[None, :]
        k2 = fx * fx + fy * fy
        acc = np.zeros_like(p)
        spectra = [np.fft.rfft2(p[..., c]) for c in range(3)]
        for sigma, wgt in zip(sigmas, weights):
            g = np.exp(-2.0 * (math.pi ** 2) * (sigma * sigma) * k2)
            for c in range(3):
                acc[..., c] += wgt * np.fft.irfft2(spectra[c] * g, s=(H, W))
        bl = np.maximum(acc[pad:-pad, pad:-pad], 0.0)
        scene = (self.hdr + bl * bloom + self.bg) * exposure
        out = tonemap(scene)
        for mask, x0, y0, colour in self.post:
            h, w = mask.shape
            region = out[y0:y0 + h, x0:x0 + w]
            region[:] = region * (1.0 - mask[..., None]) + colour[None, None, :] * mask[..., None]
        enc = srgb_encode(out)
        noise = (np.random.default_rng(7).random(enc.shape[:2])[..., None] - 0.5) / 255.0
        img = np.clip(enc + noise, 0.0, 1.0)
        return Image.fromarray((img * 255.0 + 0.5).astype(np.uint8), "RGB")

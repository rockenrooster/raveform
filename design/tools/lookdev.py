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

# Bloom: six Gaussian scales (sigma in reference pixels, 1 = screen height / 1080) and their weights.
BLOOM_SIGMAS = (1.5, 3.0, 6.0, 12.0, 24.0, 48.0)
BLOOM_WEIGHTS = (0.26, 0.22, 0.18, 0.14, 0.11, 0.09)


def tonemap(c, white_point=TONEMAP_WHITE, bleed=TONEMAP_BLEED_AMOUNT):
    """Hue-preserving: compress the brightest channel with an extended Reinhard curve, scale RGB by
    the same factor, then bleed very hot values toward white so cores go white and halos keep colour."""
    c = np.maximum(c, 0.0)
    m = c.max(axis=2, keepdims=True)
    tm = m * (1.0 + m / (white_point * white_point)) / (1.0 + m)
    tm = np.minimum(tm, 1.0)
    base = c * (tm / np.maximum(m, 1e-6))
    t = np.clip((m - TONEMAP_BLEED_START) / (TONEMAP_BLEED_END - TONEMAP_BLEED_START), 0.0, 1.0)
    white = t * t * (3.0 - 2.0 * t) * bleed
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
        self.post_rim = []                            # threat layer, drawn after tone mapping: every dark rim
        self.post_core = []                           # first, then every core. Items: (mask, x0, y0, colour)
        # The same picture as flat engine primitives in draw order: see dump() and render_engine().
        self.rec = []
        self.bg_spec = None
        self._quiet = 0                               # > 0 while a compound draw records itself as one primitive
        self._stroke = 0                              # one id per lines() call: segments of one stroke join first

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
            self._record_caps(segs, colour, intensity, width)

    def _record_caps(self, segs, colour, intensity, width):
        """One engine capsule per segment. A colour field is sampled at the two end points; the engine
        interpolates between them along the capsule."""
        if self._quiet:
            return
        if callable(colour):
            xs = np.array([[s[0], s[2]] for s in segs], np.float32)
            ys = np.array([[s[1], s[3]] for s in segs], np.float32)
            cols = np.asarray(colour(xs, ys), np.float32) * intensity
        else:
            cols = np.broadcast_to(np.asarray(colour, np.float32) * intensity, (len(segs), 2, 3))
        self._stroke += 1
        for s, c in zip(segs, cols):
            self.rec.append(("cap", float(s[0]), float(s[1]), float(s[2]), float(s[3]), float(width),
                             tuple(float(v) for v in c[0]), tuple(float(v) for v in c[1]), self._stroke))

    def joints(self):
        """Where a stroke runs on nearly straight into another capsule of similar width, both ends are
        cut flat instead of rounded, so the joint is not lit twice (design/look.md §9). Sharper corners
        keep their round, slightly hot joint. Returns one flag per record: bit 0 = flat at the first
        point, bit 1 = flat at the second."""
        flags = [0] * len(self.rec)
        limit = -math.cos(math.radians(JOINT_MAX_TURN))

        def match(same_stroke):
            ends = {}
            for i, p in enumerate(self.rec):
                if p[0] != "cap":
                    continue
                dx, dy = p[3] - p[1], p[4] - p[2]
                length = math.hypot(dx, dy)
                if length < 0.5:
                    continue                                   # dots stay round
                ux, uy = dx / length, dy / length
                for bit, x, y, ax, ay in ((1, p[1], p[2], ux, uy), (2, p[3], p[4], -ux, -uy)):
                    if not flags[i] & bit:
                        key = (p[8] if same_stroke else 0, round(x * 20.0), round(y * 20.0))
                        ends.setdefault(key, []).append((i, bit, ax, ay, p[5]))
            for group in ends.values():
                if len(group) < 2:
                    continue                                   # a free end
                # Where several lines meet (a lattice node), pair the ends that run straight through.
                pairs = []
                for a in range(len(group)):
                    for b in range(a + 1, len(group)):
                        (i, _, ax, ay, wa), (j, _, bx, by, wb) = group[a], group[b]
                        straight = ax * bx + ay * by
                        if i != j and straight <= limit and max(wa, wb) <= 1.25 * min(wa, wb):
                            pairs.append((straight, a, b))
                used = set()
                for _, a, b in sorted(pairs):
                    if a in used or b in used:
                        continue
                    used.update((a, b))
                    flags[group[a][0]] |= group[a][1]
                    flags[group[b][0]] |= group[b][1]

        match(True)
        match(False)
        return flags

    def fill(self, points, colour, intensity=0.1):
        """Faint additive fill of a closed polygon: the glass inside frames and special shapes."""
        points = list(points)
        self._add(self._poly_mask(points), colour, intensity)
        if self._quiet:
            return
        if callable(colour):
            xs = np.array([p[0] for p in points], np.float32)
            ys = np.array([p[1] for p in points], np.float32)
            cols = np.asarray(colour(xs, ys), np.float32) * intensity
        else:
            cols = np.broadcast_to(np.asarray(colour, np.float32) * intensity, (len(points), 3))
        self.rec.append(("fill", [(float(p[0]), float(p[1])) for p in points], [tuple(float(v) for v in c) for c in cols]))

    def discs(self, items, colour, intensity=1.0):
        """Filled circles (pressed buttons, thumb pads). items: iterable of (x, y, radius)."""
        for x, y, r in items:
            n = max(16, int(r))
            self._quiet += 1
            self.fill([(x + r * math.cos(2 * math.pi * k / n), y + r * math.sin(2 * math.pi * k / n)) for k in range(n)], colour, intensity)
            self._quiet -= 1
            if not self._quiet:
                c = colour(np.array([x], np.float32), np.array([y], np.float32))[0] if callable(colour) else colour
                self.rec.append(("disc", float(x), float(y), float(r), tuple(float(v) for v in np.asarray(c, np.float32) * intensity)))

    def glow(self, x, y, radius, colour, intensity=1.0):
        """Soft round glow sprite: Gaussian falling to about 5% at `radius`."""
        if not self._quiet:
            self.rec.append(("glow", float(x), float(y), float(radius), tuple(float(v) for v in np.asarray(colour, np.float32) * intensity)))
        _add_glow(self.hdr, x, y, radius, np.asarray(colour, np.float32) * intensity)

    def flare(self, x, y, length, colour, intensity=2.0, width=1.4, arms=4, rot=0.0):
        """Star streaks that taper to nothing: lens flares on pickups and special objects. Each half
        arm is two segments so that two-colour capsules can follow the fade."""
        segs = []
        for k in range(arms):
            a = rot + math.pi * k / arms
            dx, dy = math.cos(a) * length, math.sin(a) * length
            for sign in (-1.0, 1.0):
                mx, my = x + sign * dx * 0.5, y + sign * dy * 0.5
                segs.append((x, y, mx, my))
                segs.append((mx, my, x + sign * dx, y + sign * dy))
        self.lines(segs, f_fade(f_solid(colour), x, y, length, 1.6), intensity, width)

    def threat(self, segs, colour, width=2.2, rim=1.6):
        """Threat-layer draw: after tone mapping, alpha blended, with a dark rim. All rims are drawn
        before all cores, so one threat's rim never cuts into another threat's core."""
        segs = list(segs)
        if not segs:
            return
        res = self._mask(segs, width + 2 * rim)
        if res is not None:
            self.post_rim.append((res[0], res[1], res[2], np.zeros(3, np.float32)))
        res = self._mask(segs, width)
        if res is not None:
            self.post_core.append((res[0], res[1], res[2], np.asarray(colour, np.float32)))
        if not self._quiet:
            c = tuple(float(v) for v in np.asarray(colour, np.float32))
            for s in segs:
                self.rec.append(("thr", float(s[0]), float(s[1]), float(s[2]), float(s[3]), float(width), float(rim), c))

    def text(self, font, s, x, y, cap_height, colour, intensity=1.0, width=None):
        unit = cap_height / 6.0
        width = width if width is not None else max(1.2, 0.55 * unit)
        segs, end = text_segments(font, s, x, y, cap_height)
        self.lines(segs, colour, intensity, width)
        return end

    # ---- background (does not bloom)

    def radial_bg(self, edge, centre, cx=0.5, cy=0.5, radius=0.8, power=1.5):
        yy, xx = np.mgrid[0:self.h, 0:self.w].astype(np.float32) + 0.5      # pixel centres, like every primitive
        d = np.sqrt(((xx / self.w - cx) * self.w / self.h) ** 2 + (yy / self.h - cy) ** 2) / radius
        t = (np.clip(1.0 - d, 0.0, 1.0) ** power)[..., None]
        self.bg = np.asarray(edge, np.float32)[None, None, :] * (1 - t) + np.asarray(centre, np.float32)[None, None, :] * t
        self.bg_spec = (tuple(float(v) for v in edge), tuple(float(v) for v in centre), cx, cy, radius, power)

    def nebula(self, blobs):
        """Soft colour clouds behind everything. blobs: iterable of (x, y, radius, colour, intensity)."""
        for x, y, radius, colour, intensity in blobs:
            self.rec.append(("neb", float(x), float(y), float(radius), tuple(float(v) for v in np.asarray(colour, np.float32) * intensity)))
            box = self._box([x], [y], radius * 1.3)
            if box is None:
                continue
            x0, y0, x1, y1 = box
            yy, xx = np.mgrid[y0:y1, x0:x1].astype(np.float32) + 0.5
            sigma = radius / 2.2
            g = np.exp(-((xx - x) ** 2 + (yy - y) ** 2) / (2 * sigma * sigma))
            self.bg[y0:y1, x0:x1] += g[..., None] * (np.asarray(colour, np.float32) * intensity)

    # ---- composite

    def _composite(self, hdr, rims, cores, bloom=0.9, exposure=1.0, sigmas=BLOOM_SIGMAS, weights=BLOOM_WEIGHTS,
                   bg_level=1.0, white_point=TONEMAP_WHITE, bleed=TONEMAP_BLEED_AMOUNT):
        pad = 128
        p = np.pad(hdr, ((pad, pad), (pad, pad), (0, 0)), mode="constant")
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
        scene = (hdr + bl * bloom + self.bg * bg_level) * exposure
        out = tonemap(scene, white_point, bleed)
        for mask, x0, y0, colour in list(rims) + list(cores):
            h, w = mask.shape
            region = out[y0:y0 + h, x0:x0 + w]
            region[:] = region * (1.0 - mask[..., None]) + colour[None, None, :] * mask[..., None]
        enc = srgb_encode(out)
        noise = (np.random.default_rng(7).random(enc.shape[:2])[..., None] - 0.5) / 255.0
        img = np.clip(enc + noise, 0.0, 1.0)
        return Image.fromarray((img * 255.0 + 0.5).astype(np.uint8), "RGB")

    def render(self, **look):
        """The look-dev picture. Keyword arguments: bloom, exposure, sigmas, weights, bg_level,
        white_point, bleed (defaults are the reference look in design/look.md)."""
        return self._composite(self.hdr, self.post_rim, self.post_core, **look)

    def engine_hdr(self, width_scale=1.0, pool_level=1.0):
        """The scene a correct engine draws from the recorded primitives: every capsule is its own
        additive instance with the engine's coverage (a one-pixel anti-aliased edge) and a two-colour
        gradient along its axis. `width_scale` and `pool_level` are the look-variant controls for line
        width and for the brightness of pools (glows of POOL_MIN_RADIUS and up).
        Returns (hdr, threat rims, threat cores)."""
        hdr = np.zeros((self.h, self.w, 3), np.float32)
        rims, cores = [], []
        black = np.zeros(3, np.float32)
        flags = self.joints()
        for i, p in enumerate(self.rec):
            kind = p[0]
            if kind == "cap":
                _add_capsule(hdr, p[1], p[2], p[3], p[4], p[5] * 0.5 * width_scale, p[6], p[7], flags[i])
            elif kind == "disc":
                _add_capsule(hdr, p[1], p[2], p[1], p[2], p[3], p[4], p[4])
            elif kind == "glow":
                _add_glow(hdr, p[1], p[2], p[3], np.asarray(p[4], np.float32) * (pool_level if p[3] >= POOL_MIN_RADIUS else 1.0))
            elif kind == "fill":                              # flat triangles; the mean colour is close enough here
                res = self._poly_mask(p[1])
                if res is not None:
                    a, x0, y0 = res
                    hdr[y0:y0 + a.shape[0], x0:x0 + a.shape[1]] += a[..., None] * np.mean(np.asarray(p[2], np.float32), axis=0)
            elif kind == "thr":
                rim = _capsule_mask(self.w, self.h, p[1], p[2], p[3], p[4], p[5] * 0.5 + p[6])
                core = _capsule_mask(self.w, self.h, p[1], p[2], p[3], p[4], p[5] * 0.5)
                if rim is not None:
                    rims.append(rim + (black,))
                if core is not None:
                    cores.append(core + (np.asarray(p[7], np.float32),))
        return hdr, rims, cores

    def render_engine(self, **look):
        hdr, rims, cores = self.engine_hdr()
        return self._composite(hdr, rims, cores, **look)

    def dump(self, path, bloom=0.9, exposure=1.0):
        """Write the recorded primitives as a text scene the engine replays (format: design/look.md §9).
        Colours are linear-light RGB already multiplied by their HDR intensity."""
        def xy(*v):
            return " ".join(("%.2f" % a).rstrip("0").rstrip(".") for a in v)

        def col(c):
            return " ".join("%.4g" % v for v in c)

        out = ["rfscene 1", "size %d %d" % (self.w, self.h), "bloom %g" % bloom, "exposure %g" % exposure]
        if self.bg_spec:
            edge, centre, cx, cy, radius, power = self.bg_spec
            out.append("bg %s %s %g %g %g %g" % (col(edge), col(centre), cx, cy, radius, power))
        counts = {}
        flags = self.joints()
        for i, p in enumerate(self.rec):
            kind = p[0]
            counts[kind] = counts.get(kind, 0) + 1
            if kind == "cap":
                tail = col(p[6]) if p[6] == p[7] else col(p[6]) + " " + col(p[7])
                out.append("cap %s %g %d %s" % (xy(p[1], p[2], p[3], p[4]), p[5], flags[i], tail))
            elif kind == "disc":
                out.append("disc %s %s" % (xy(p[1], p[2], p[3]), col(p[4])))
            elif kind == "glow":
                out.append("glow %s %s" % (xy(p[1], p[2], p[3]), col(p[4])))
            elif kind == "neb":
                out.append("neb %s %s" % (xy(p[1], p[2], p[3]), col(p[4])))
            elif kind == "fill":
                for a, b, c in triangulate(p[1]):
                    out.append("tri %s %s %s %s %s %s" % (xy(*p[1][a]), col(p[2][a]), xy(*p[1][b]), col(p[2][b]),
                                                          xy(*p[1][c]), col(p[2][c])))
            elif kind == "thr":
                out.append("thr %s %g %g %s" % (xy(p[1], p[2], p[3], p[4]), p[5], p[6], col(p[7])))
        with open(path, "w", encoding="utf-8", newline="\n") as f:
            f.write("\n".join(out) + "\n")
        return counts


# --------------------------------------------------------------------------- engine-exact primitives

def _clip_box(w, h, xs, ys, pad):
    x0 = max(0, int(math.floor(min(xs) - pad)))
    y0 = max(0, int(math.floor(min(ys) - pad)))
    x1 = min(w, int(math.ceil(max(xs) + pad)) + 1)
    y1 = min(h, int(math.ceil(max(ys) + pad)) + 1)
    return (x0, y0, x1, y1) if x1 > x0 and y1 > y0 else None


JOINT_MAX_TURN = 20.0      # degrees: gentler turns between two capsules get a flat joint, sharper ones a round one
POOL_MIN_RADIUS = 16.0     # reference pixels: a glow this large is a pool (may be drawn at low resolution)


def _capsule_cover(w, h, x0, y0, x1, y1, radius, flags=0):
    """Coverage of one capsule with the engine's formula clamp(radius + 0.5 - distance, 0, 1), sampled
    at pixel centres. A flat end (flags bit 0 at the first point, bit 1 at the second) has no cap: the
    coverage is cut there with a one-pixel ramp, so two flat ends that meet add up to exactly one.
    Returns (coverage, t along the axis, bx0, by0) or None when off screen."""
    box = _clip_box(w, h, (x0, x1), (y0, y1), radius + 1.5)
    if box is None:
        return None
    bx0, by0, bx1, by1 = box
    yy, xx = np.mgrid[by0:by1, bx0:bx1].astype(np.float32)
    px, py = xx + 0.5 - x0, yy + 0.5 - y0
    dx, dy = x1 - x0, y1 - y0
    length = math.hypot(dx, dy)
    cut = 1.0
    if length > 1e-4:
        ux, uy = dx / length, dy / length
        along = px * ux + py * uy
        across = py * ux - px * uy
        beyond = np.zeros_like(along)
        if flags & 1:
            cut = cut * np.clip(along + 0.5, 0.0, 1.0)
        else:
            beyond = np.maximum(beyond, -along)
        if flags & 2:
            cut = cut * np.clip(length - along + 0.5, 0.0, 1.0)
        else:
            beyond = np.maximum(beyond, along - length)
        dist = np.hypot(beyond, across)
        t = np.clip(along / length, 0.0, 1.0)
    else:
        dist = np.hypot(px, py)
        t = np.zeros_like(dist)
    return np.clip(radius + 0.5 - dist, 0.0, 1.0) * cut, t, bx0, by0


def _add_capsule(hdr, x0, y0, x1, y1, radius, c0, c1, flags=0):
    res = _capsule_cover(hdr.shape[1], hdr.shape[0], x0, y0, x1, y1, radius, flags)
    if res is None:
        return
    cover, t, bx0, by0 = res
    c0, c1 = np.asarray(c0, np.float32), np.asarray(c1, np.float32)
    colour = c0[None, None, :] + (c1 - c0)[None, None, :] * t[..., None]
    hdr[by0:by0 + cover.shape[0], bx0:bx0 + cover.shape[1]] += cover[..., None] * colour


def _capsule_mask(w, h, x0, y0, x1, y1, radius):
    res = _capsule_cover(w, h, x0, y0, x1, y1, radius)
    return None if res is None else (res[0], res[2], res[3])


def _add_glow(hdr, x, y, radius, colour):
    """Gaussian with sigma = radius / 2.45 (about 5% at `radius`), cut off at 1.25 radius.

    It fades to zero over the last fifth of the cut-off along a circle (1 - smoothstep(0.8 cut, cut, r)),
    as shaders/glow.frag does since M3 stage C: a very bright light used to show its square box.
    """
    cut = radius * 1.25
    box = _clip_box(hdr.shape[1], hdr.shape[0], (x,), (y,), cut)
    if box is None:
        return
    x0, y0, x1, y1 = box
    yy, xx = np.mgrid[y0:y1, x0:x1].astype(np.float32)
    sigma = radius / 2.45
    d2 = (xx + 0.5 - x) ** 2 + (yy + 0.5 - y) ** 2                                    # sampled at pixel centres
    u = np.clip((np.sqrt(d2) - 0.8 * cut) / (0.2 * cut), 0.0, 1.0)
    g = np.exp(-d2 / (2 * sigma * sigma)) * (1.0 - u * u * (3.0 - 2.0 * u))
    hdr[y0:y1, x0:x1] += g[..., None] * np.asarray(colour, np.float32)


def triangulate(points):
    """Ear clipping for a simple polygon. Returns index triples."""
    n = len(points)
    if n < 3:
        return []
    idx = list(range(n))
    if sum(points[i][0] * points[(i + 1) % n][1] - points[(i + 1) % n][0] * points[i][1] for i in range(n)) < 0:
        idx.reverse()

    def cross(o, a, b):
        return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])

    def inside(q, a, b, c):
        return cross(a, b, q) >= 0 and cross(b, c, q) >= 0 and cross(c, a, q) >= 0

    tris = []
    while len(idx) > 3:
        m = len(idx)
        for k in range(m):
            i0, i1, i2 = idx[(k - 1) % m], idx[k], idx[(k + 1) % m]
            a, b, c = points[i0], points[i1], points[i2]
            if cross(a, b, c) <= 1e-9:
                continue                                       # reflex or flat corner
            if any(inside(points[j], a, b, c) for j in idx if j not in (i0, i1, i2)):
                continue
            tris.append((i0, i1, i2))
            del idx[k]
            break
        else:
            break                                              # no ear (self-intersecting outline): fan the rest
    tris += [(idx[0], idx[k], idx[k + 1]) for k in range(1, len(idx) - 1)]
    return tris

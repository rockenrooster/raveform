"""RAVEFORM palettes: recipes (edit here), export to design/palettes.json, preview sheet.

Usage:  python design/tools/palettes.py            -> writes design/palettes.json and design/previews/palettes.png
"""
import json
import math
import os
import random
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import lookdev as ld  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DESIGN = os.path.join(ROOT, "design")

# --------------------------------------------------------------------------- recipes
# Colours are OKLCH: (L, hue) plus either "c" (absolute chroma) or "cf" (fraction of the sRGB gamut
# maximum at that lightness and hue; neon colours sit near 1.0).

CONSTANTS = {
    # Reserved for anything that hurts at range. Nothing else may use hues within 30 degrees of it.
    "danger": dict(L=0.66, h=18, cf=0.98),
    # Each player is the only two-tone object in the game: `deep` is the far end of the ship's gradient
    # and trail, `light` the second colour of its halo and Riff rings.
    "players": [
        dict(name="P1 cyan", L=0.86, h=200, cf=0.95, deep=dict(L=0.64, h=262, cf=0.96), light=dict(L=0.80, h=226, cf=0.96)),
        dict(name="P2 lime", L=0.88, h=132, cf=0.95, deep=dict(L=0.66, h=150, cf=0.96), light=dict(L=0.93, h=118, cf=0.90)),
        dict(name="P3 violet", L=0.70, h=302, cf=0.95, deep=dict(L=0.56, h=282, cf=0.96), light=dict(L=0.80, h=318, cf=0.90)),
        dict(name="P4 gold", L=0.86, h=88, cf=0.95, deep=dict(L=0.74, h=62, cf=0.96), light=dict(L=0.93, h=100, cf=0.85)),
    ],
    "pickup": dict(L=0.95, h=95, c=0.07),
    "white": dict(L=1.0, h=0, c=0.0),
}

PALETTES = [
    dict(id="ultraviolet", name="Ultraviolet", track=1,
         bg0=dict(L=0.10, h=285, c=0.045), bg1=dict(L=0.17, h=300, c=0.085),
         grid=dict(L=0.42, h=280, c=0.14), gridHot=dict(L=0.88, h=280, c=0.07),
         accent=dict(L=0.80, h=320, cf=0.9),
         enemies=[dict(L=0.70, h=300, cf=0.97), dict(L=0.66, h=268, cf=0.97), dict(L=0.72, h=318, cf=0.95),
                  dict(L=0.80, h=172, cf=0.95), dict(L=0.76, h=238, cf=0.95)]),
    # 2026-11-15 (the gold rule, below): the sun is the accent's; the crowd a flare's tangerine (warm as Furnace's and
    # Afterglow's, as it always was), the hunters lime (P2's lime was already among Solar's hues: h118). The old amber
    # (h70) and yellow (h95) stay listed last, out of the gold's way, as the record of what Solar was.
    dict(id="solar", name="Solar", track=2,
         bg0=dict(L=0.10, h=200, c=0.030), bg1=dict(L=0.17, h=185, c=0.050),
         grid=dict(L=0.42, h=190, c=0.08), gridHot=dict(L=0.88, h=190, c=0.06),
         accent=dict(L=0.90, h=100, cf=0.9),
         enemies=[dict(L=0.80, h=55, cf=0.97), dict(L=0.88, h=122, cf=0.95), dict(L=0.70, h=292, cf=0.95),
                  dict(L=0.80, h=70, cf=0.97), dict(L=0.88, h=95, cf=0.97)]),
    dict(id="reef", name="Reef", track=3,
         bg0=dict(L=0.10, h=250, c=0.045), bg1=dict(L=0.18, h=235, c=0.070),
         grid=dict(L=0.45, h=215, c=0.10), gridHot=dict(L=0.88, h=215, c=0.07),
         accent=dict(L=0.88, h=150, cf=0.9),
         enemies=[dict(L=0.86, h=158, cf=0.97), dict(L=0.88, h=128, cf=0.95), dict(L=0.70, h=310, cf=0.95),
                  dict(L=0.90, h=102, cf=0.95), dict(L=0.66, h=262, cf=0.95)]),
    dict(id="aurora", name="Aurora", track=4,
         bg0=dict(L=0.09, h=265, c=0.040), bg1=dict(L=0.17, h=170, c=0.060),
         grid=dict(L=0.44, h=165, c=0.10), gridHot=dict(L=0.88, h=165, c=0.06),
         accent=dict(L=0.82, h=300, cf=0.9),
         enemies=[dict(L=0.86, h=150, cf=0.97), dict(L=0.70, h=295, cf=0.97), dict(L=0.74, h=322, cf=0.95),
                  dict(L=0.80, h=232, cf=0.95), dict(L=0.90, h=115, cf=0.95)]),
    # 2026-11-15 (the gold rule): an ember crowd, blue-flame hunters, violet-smoke specials; the white-hot hunters (h98, the
    # pickup's own colour) are gone.
    dict(id="furnace", name="Furnace", track=5,
         bg0=dict(L=0.09, h=60, c=0.012), bg1=dict(L=0.16, h=55, c=0.038),
         grid=dict(L=0.40, h=60, c=0.085), gridHot=dict(L=0.88, h=60, c=0.06),
         accent=dict(L=0.80, h=250, cf=0.9),
         enemies=[dict(L=0.74, h=58, cf=0.97), dict(L=0.68, h=258, cf=0.97), dict(L=0.70, h=300, cf=0.95),
                  dict(L=0.93, h=98, c=0.08), dict(L=0.90, h=108, cf=0.95)]),
    # 2026-11-15 (the gold rule; the owner: "Track 6's enemies are the same colour as the pickups"): noon's crowd is the white
    # of the sky, cool, where it was the pickups' cream; the hunters teal, the specials violet.
    dict(id="zenith", name="Zenith", track=6,
         bg0=dict(L=0.08, h=260, c=0.020), bg1=dict(L=0.16, h=255, c=0.040),
         grid=dict(L=0.50, h=250, c=0.06), gridHot=dict(L=0.95, h=240, c=0.05),
         accent=dict(L=0.97, h=90, c=0.04),
         enemies=[dict(L=0.90, h=240, c=0.07), dict(L=0.88, h=165, cf=0.95), dict(L=0.66, h=288, cf=0.97),
                  dict(L=0.70, h=258, cf=0.97), dict(L=0.95, h=95, c=0.06)]),
    # Tracks 7 to 10 (2026-11-12, when the set list grew to ten).
    # Afterglow: dusk. Peach-gold and lavender on plum; the first major key gets the warmest sky.
    # (2026-11-15, the gold rule: the peach moves off the gold to coral; the sky keeps its gold in the accent)
    dict(id="afterglow", name="Afterglow", track=7,
         bg0=dict(L=0.10, h=320, c=0.035), bg1=dict(L=0.18, h=340, c=0.060),
         grid=dict(L=0.44, h=310, c=0.09), gridHot=dict(L=0.88, h=310, c=0.06),
         accent=dict(L=0.88, h=75, cf=0.9),
         enemies=[dict(L=0.78, h=50, cf=0.97), dict(L=0.72, h=300, cf=0.97), dict(L=0.72, h=250, cf=0.95),
                  dict(L=0.86, h=168, cf=0.95), dict(L=0.84, h=72, cf=0.97)]),
    # Arcadia: the arcade. Saturated primaries on near-black, a green grid.
    dict(id="arcadia", name="Arcadia", track=8,
         bg0=dict(L=0.07, h=270, c=0.015), bg1=dict(L=0.14, h=265, c=0.035),
         grid=dict(L=0.45, h=145, c=0.12), gridHot=dict(L=0.90, h=145, c=0.07),
         accent=dict(L=0.90, h=100, cf=0.95),
         enemies=[dict(L=0.86, h=145, cf=0.97), dict(L=0.70, h=250, cf=0.97), dict(L=0.72, h=320, cf=0.95),
                  dict(L=0.92, h=95, cf=0.95), dict(L=0.86, h=190, cf=0.95)]),
    # Mirage: turquoise and sand over deep teal; the grid is the sand.
    # (2026-11-15, the gold rule: the sand stays the grid's; the crowd is the oasis's green, 40 degrees clear of P1's cyan)
    dict(id="mirage", name="Mirage", track=9,
         bg0=dict(L=0.10, h=215, c=0.030), bg1=dict(L=0.19, h=195, c=0.055),
         grid=dict(L=0.46, h=80, c=0.07), gridHot=dict(L=0.90, h=85, c=0.06),
         accent=dict(L=0.86, h=180, cf=0.9),
         enemies=[dict(L=0.86, h=160, cf=0.97), dict(L=0.70, h=285, cf=0.97), dict(L=0.78, h=235, cf=0.95),
                  dict(L=0.86, h=180, cf=0.97), dict(L=0.88, h=80, cf=0.95)]),
    # Last Light: Ultraviolet's violets again, with gold in them and a warm horizon: the first Track at dawn.
    # (2026-11-15, the gold rule: the gold is the horizon's and the accent's now; the hunters are dawn's rose)
    dict(id="lastlight", name="Last Light", track=10,
         bg0=dict(L=0.09, h=280, c=0.040), bg1=dict(L=0.19, h=60, c=0.050),
         grid=dict(L=0.44, h=275, c=0.11), gridHot=dict(L=0.90, h=80, c=0.06),
         accent=dict(L=0.92, h=90, cf=0.9),
         enemies=[dict(L=0.70, h=290, cf=0.97), dict(L=0.74, h=325, cf=0.95), dict(L=0.72, h=250, cf=0.97),
                  dict(L=0.86, h=165, cf=0.95), dict(L=0.90, h=85, cf=0.95)]),
    # (M8 part 4: the two bonus Tracks' palettes, as design/modes/README.md 11 writes them)
    # Mirrorball (bonus Track 11): a night club. Silver on midnight; the colours are the lights' own.
    dict(id="mirrorball", name="Mirrorball", track=11,
         bg0=dict(L=0.08, h=275, c=0.030), bg1=dict(L=0.17, h=290, c=0.060),
         grid=dict(L=0.50, h=285, c=0.045), gridHot=dict(L=0.95, h=285, c=0.035),
         accent=dict(L=0.96, h=280, c=0.03),
         enemies=[dict(L=0.84, h=150, cf=0.97), dict(L=0.68, h=258, cf=0.97), dict(L=0.72, h=318, cf=0.95),
                  dict(L=0.80, h=55, cf=0.95), dict(L=0.86, h=172, cf=0.95)]),
    # Toybox (bonus Track 12): crayons on a dark blanket. Softer than Arcadia's primaries: pastel, cf under 0.8.
    dict(id="toybox", name="Toybox", track=12,
         bg0=dict(L=0.11, h=275, c=0.035), bg1=dict(L=0.20, h=250, c=0.060),
         grid=dict(L=0.48, h=265, c=0.080), gridHot=dict(L=0.92, h=265, c=0.050),
         accent=dict(L=0.90, h=160, cf=0.60),
         enemies=[dict(L=0.82, h=245, cf=0.75), dict(L=0.80, h=55, cf=0.80), dict(L=0.86, h=170, cf=0.70),
                  dict(L=0.84, h=150, cf=0.75), dict(L=0.76, h=295, cf=0.75)]),
]

MIN_HUE_GAP_DANGER = 30.0
MIN_HUE_GAP_PLAYER = 25.0
LOW_CHROMA = 0.09  # near-white colours ignore the danger and player rules (not the gold rule)
PARTNER_HUE_STEP = 40.0   # an enemy hue's gradient partner sits this far along the colour wheel
# The gold rule (2026-11-15; the owner, on the review: "Track 6's enemies are the same colour as the pickups. Tracks 5 and 7
# are close. fix"). What the game gives is gold in every run, solo too: Shards and the Trio's orbs in the pickup white-gold,
# Boost orbs, Cells, the Extra Life, Vaults and the level-up Trio's ring in P4's gold. No enemy hue comes within
# MIN_HUE_GAP_GOLD of either gold's hue, whatever its chroma (a warm near-white is the pickup's own colour), and a partner
# keeps clear of them as well (where neither of its two hues can, it is the hue itself, lighter).
MIN_HUE_GAP_GOLD = 25.0


def gold_hues():
    return [CONSTANTS["players"][3]["h"], CONSTANTS["pickup"]["h"]]


def gold_gap(h):
    return min(ld.hue_distance(h, g) for g in gold_hues())


def resolve(spec):
    rgb, c = ld.oklch(spec["L"], spec["h"], c=spec.get("c"), cf=spec.get("cf"))
    return dict(oklch=[round(spec["L"], 4), round(float(c), 4), float(spec["h"])],
                linear=[round(float(v), 5) for v in rgb], hex=ld.hexstr(rgb))


def partners(entry):
    """The candidate partner colours of an enemy hue (the far end of its hull gradient): the hue moved
    PARTNER_HUE_STEP either way, slightly lighter, and (third) the hue itself, lighter, for when neither
    of the first two keeps clear (pick_partner). A near-white's partners are its own hue, pale."""
    L, c, h = entry["oklch"]
    if c < LOW_CHROMA:
        return [resolve(dict(L=0.86, h=h, cf=0.6))] * 3
    moved = [resolve(dict(L=min(0.9, L + 0.05), h=(h + step) % 360.0, cf=0.95)) for step in (PARTNER_HUE_STEP, -PARTNER_HUE_STEP)]
    return moved + [resolve(dict(L=min(0.92, L + 0.08), h=h, cf=0.8))]


def pick_partner(options, player_hues, danger_hue):
    """Of the two moved partners, the one that keeps clear of the danger hue, every present player's hue and
    the golds, by the larger margin; where neither keeps clear, the third (the hue itself, lighter)."""
    def margin(option):
        h = option["oklch"][2]
        return min([ld.hue_distance(h, danger_hue) - MIN_HUE_GAP_DANGER, gold_gap(h) - MIN_HUE_GAP_GOLD] +
                   [ld.hue_distance(h, ph) - MIN_HUE_GAP_PLAYER for ph in player_hues])
    m0, m1 = margin(options[0]), margin(options[1])
    if max(m0, m1) < 0.0 and len(options) > 2:
        return 2
    return 0 if m0 >= m1 else 1


def pick_enemy_hues(candidates, player_hues, danger_hue, count=3):
    """Designer order is preference order. Skip a candidate that sits too close to the danger hue, to
    a present player's hue or to the golds; if fewer than `count` survive, fill with the least-bad remaining ones."""
    ok, rest = [], []
    for i, cand in enumerate(candidates):
        L, c, h = cand["oklch"]
        gap_g = gold_gap(h)
        if c < LOW_CHROMA:
            if gap_g >= MIN_HUE_GAP_GOLD:
                ok.append(i)
            else:
                rest.append((gap_g - MIN_HUE_GAP_GOLD, i))
            continue
        gap_d = ld.hue_distance(h, danger_hue)
        gap_p = min([ld.hue_distance(h, ph) for ph in player_hues] or [360.0])
        if gap_d >= MIN_HUE_GAP_DANGER and gap_p >= MIN_HUE_GAP_PLAYER and gap_g >= MIN_HUE_GAP_GOLD:
            ok.append(i)
        else:
            rest.append((min(gap_d - MIN_HUE_GAP_DANGER, gap_p - MIN_HUE_GAP_PLAYER, gap_g - MIN_HUE_GAP_GOLD), i))
    rest.sort(reverse=True)
    chosen = ok[:count]
    for _, i in rest:
        if len(chosen) >= count:
            break
        chosen.append(i)
    return sorted(chosen)


def build():
    const = dict(
        danger=resolve(CONSTANTS["danger"]),
        players=[dict(name=p["name"], **resolve(p), deep=resolve(p["deep"]), light=resolve(p["light"])) for p in CONSTANTS["players"]],
        pickup=resolve(CONSTANTS["pickup"]),
        white=resolve(CONSTANTS["white"]),
    )
    danger_hue = const["danger"]["oklch"][2]
    player_hues = [p["oklch"][2] for p in const["players"]]
    out = []
    for p in PALETTES:
        entry = dict(id=p["id"], name=p["name"], track=p["track"])
        for role in ("bg0", "bg1", "grid", "gridHot", "accent"):
            entry[role] = resolve(p[role])
        entry["enemyCandidates"] = [resolve(e) for e in p["enemies"]]
        for cand in entry["enemyCandidates"]:
            cand["partners"] = partners(cand)
            cand["soloPartner"] = pick_partner(cand["partners"], player_hues[:1], danger_hue)
            cand["fourPlayerPartner"] = pick_partner(cand["partners"], player_hues, danger_hue)
        entry["soloEnemies"] = pick_enemy_hues(entry["enemyCandidates"], player_hues[:1], danger_hue)
        entry["fourPlayerEnemies"] = pick_enemy_hues(entry["enemyCandidates"], player_hues, danger_hue)
        out.append(entry)
    return dict(
        note="Generated by design/tools/palettes.py. linear = linear-light sRGB 0..1, multiply by an HDR "
             "intensity from design/look.md. Enemy hues per run: see selectionRule; their gradient partners: "
             "see partnerRule. players[i].deep and .light are that player's second and third colours.",
        selectionRule=dict(
            text="Take enemyCandidates in listed order; skip any whose hue is within minHueGapDanger of the "
                 "danger hue or within minHueGapPlayer of a present player's hue (colours with chroma below "
                 "lowChroma are exempt from those two), or within minHueGapGold of the gifts' golds (P4's hue "
                 "and the pickup's, in every run, whatever the chroma: 2026-11-15); use the first three that "
                 "remain; if fewer than three remain, fill with the skipped candidates that violate the gaps least.",
            minHueGapDanger=MIN_HUE_GAP_DANGER, minHueGapPlayer=MIN_HUE_GAP_PLAYER, lowChroma=LOW_CHROMA,
            minHueGapGold=MIN_HUE_GAP_GOLD, goldHues=gold_hues()),
        partnerRule=dict(
            text="Every enemy hue has three candidate partners (enemyCandidates[i].partners): its hue moved "
                 "hueStep degrees up or down the wheel, and the hue itself, lighter. Use whichever of the first "
                 "two keeps clear of the danger hue, every present player's hue and the golds by the larger "
                 "margin (on a tie the first); if neither keeps clear, the third. soloPartner and "
                 "fourPlayerPartner are that choice precomputed for P1 alone and for all four players.",
            hueStep=PARTNER_HUE_STEP),
        constants=const, palettes=out)


# --------------------------------------------------------------------------- preview

def lin(entry):
    return np.asarray(entry["linear"], np.float32)


def draw_panel(data, pal, font, w=800, h=450, seed=1):
    rng = random.Random(seed)
    cv = ld.Canvas(w, h)
    const = data["constants"]
    cv.radial_bg(lin(pal["bg0"]) * 0.55, lin(pal["bg1"]) * 0.55, cx=0.42, cy=0.5)
    enemies = [lin(pal["enemyCandidates"][i]) for i in pal["soloEnemies"]]
    white = np.ones(3, np.float32)
    p1 = lin(const["players"][0])
    danger = lin(const["danger"])
    gold = lin(const["pickup"])

    # warped grid: an explosion pushes, a well pulls
    boom = (0.66 * w, 0.42 * h)
    well = (0.24 * w, 0.74 * h)

    def warp(x, y):
        dx, dy = x - boom[0], y - boom[1]
        d = math.hypot(dx, dy) + 1e-3
        push = 26.0 * math.exp(-((d - 70.0) / 55.0) ** 2)
        x, y = x + dx / d * push, y + dy / d * push
        dx, dy = x - well[0], y - well[1]
        d = math.hypot(dx, dy) + 1e-3
        pull = min(d * 0.7, 900.0 / (d + 18.0))
        return x - dx / d * pull, y - dy / d * pull, push + pull

    cold, hot = [], []
    step, sub = 40, 10
    for gx in range(0, w + 1, step):
        prev = None
        for gy in range(-sub, h + sub + 1, sub):
            cur = warp(gx, gy)
            if prev:
                (hot if (cur[2] + prev[2]) * 0.5 > 9.0 else cold).append((prev[0], prev[1], cur[0], cur[1]))
            prev = cur
    for gy in range(0, h + 1, step):
        prev = None
        for gx in range(-sub, w + sub + 1, sub):
            cur = warp(gx, gy)
            if prev:
                (hot if (cur[2] + prev[2]) * 0.5 > 9.0 else cold).append((prev[0], prev[1], cur[0], cur[1]))
            prev = cur
    cv.lines(cold, lin(pal["grid"]), 0.30, 1.1)
    cv.lines(hot, lin(pal["gridHot"]), 0.75, 1.2)

    # enemies: hexagons (drifters), chevrons (hounds), stars (blinks)
    ship = (0.36 * w, 0.52 * h)
    chevron = ld.polyline([(-0.8, 0.9), (1.0, 0.0), (-0.8, -0.9), (-0.25, 0.0)], closed=True)
    for i in range(7):
        x, y = rng.uniform(0.05, 0.95) * w, rng.uniform(0.12, 0.92) * h
        if math.hypot(x - ship[0], y - ship[1]) < 90:
            continue
        r = rng.uniform(13, 17)
        cv.lines(ld.transform(ld.polygon(6), x, y, r, rng.uniform(0, 1)), enemies[0], 2.0, 1.7)
        cv.lines(ld.transform(ld.polygon(3), x, y, r * 0.45, rng.uniform(0, 2)), enemies[0], 1.4, 1.3)
    for i in range(6):
        a = rng.uniform(0, 2 * math.pi)
        d = rng.uniform(120, 300)
        x, y = ship[0] + math.cos(a) * d, ship[1] + math.sin(a) * d * 0.6
        if not (20 < x < w - 20 and 40 < y < h - 20):
            continue
        cv.lines(ld.transform(chevron, x, y, 15, math.atan2(ship[1] - y, ship[0] - x)), enemies[1], 2.2, 1.8)
    for i in range(4):
        x, y = rng.uniform(0.1, 0.9) * w, rng.uniform(0.15, 0.9) * h
        cv.lines(ld.transform(ld.star(4, 1.0, 0.38), x, y, 17, rng.uniform(0, 1)), enemies[2], 2.2, 1.6)
        cv.lines(ld.transform(ld.polygon(4), x, y, 5, 0.78), enemies[2], 1.6, 1.2)

    # gravity well rings
    for k, rr in enumerate((30, 21, 13)):
        cv.lines(ld.transform(ld.arc(1.0, 0.3 * k, 0.3 * k + 5.2, 22), well[0], well[1], rr), enemies[2], 1.3 + 0.4 * k, 1.4)

    # explosion: sparks, shock ring, shards
    for i in range(46):
        a = rng.uniform(0, 2 * math.pi)
        r0 = rng.uniform(18, 70)
        ln = rng.uniform(8, 30)
        col = enemies[0] * (1 - 0.35 * rng.random()) + white * 0.35 * rng.random()
        cv.lines([(boom[0] + math.cos(a) * r0, boom[1] + math.sin(a) * r0,
                   boom[0] + math.cos(a) * (r0 + ln), boom[1] + math.sin(a) * (r0 + ln))], col, rng.uniform(2.0, 5.5), 1.3)
    cv.lines(ld.transform(ld.polygon(40), boom[0], boom[1], 78), white, 0.9, 1.2)
    for i in range(9):
        a = rng.uniform(0, 2 * math.pi)
        d = rng.uniform(20, 60)
        cv.lines(ld.transform(ld.polygon(4), boom[0] + math.cos(a) * d, boom[1] + math.sin(a) * d, 3.2, 0.4), gold, 3.0, 1.2)

    # player: dart ship, white-hot core, trail, shots along the aim line
    aim = math.atan2(boom[1] - ship[1], boom[0] - ship[0])
    hull = ld.polyline([(1.0, 0.0), (-0.75, 0.78), (-0.3, 0.0), (-0.75, -0.78)], closed=True)
    cv.lines(ld.transform(hull, ship[0], ship[1], 15, aim), p1, 2.6, 2.0)
    cv.lines(ld.transform(hull, ship[0], ship[1], 15, aim), white, 2.2, 0.8)
    for k in range(10):
        t0, t1 = k / 10.0, (k + 1) / 10.0
        bx, by = -math.cos(aim), -math.sin(aim)
        cv.lines([(ship[0] + bx * (12 + 70 * t0), ship[1] + by * (12 + 70 * t0),
                   ship[0] + bx * (12 + 70 * t1), ship[1] + by * (12 + 70 * t1))], p1, 1.6 * (1 - t0) ** 1.5, 2.2)
    for k in range(5):
        d = 34 + k * 44
        x, y = ship[0] + math.cos(aim) * d, ship[1] + math.sin(aim) * d
        cv.lines([(x, y, x + math.cos(aim) * 13, y + math.sin(aim) * 13)], p1 * 0.45 + white * 0.55, 4.5, 2.0)

    # danger: hollow diamond bullets, drawn on the threat layer and also as light
    for k in range(7):
        a = 2.2 + k * 0.34
        d = 58 + k * 15
        x, y = ship[0] + math.cos(a) * d, ship[1] + math.sin(a) * d
        dia = ld.transform(ld.polygon(4), x, y, 5.2, 0.0)
        cv.lines(dia, danger, 2.4, 2.0)
        cv.threat(dia, danger * 1.0 + white * 0.25, 1.9, 1.3)

    cv.text(font, pal["name"].upper(), 16, 14, 20, lin(pal["accent"]), 1.5)
    cv.text(font, "TRACK %02d" % pal["track"], 16, 46, 10, white, 0.9, 1.3)
    cv.text(font, "X128", w - 110, 14, 20, gold, 1.8)
    return cv.render(bloom=0.9, exposure=1.0)


def swatch_strip(data, pal, font, w=800, h=64):
    cv = ld.Canvas(w, h)
    items = [("BG0", pal["bg0"]), ("BG1", pal["bg1"]), ("GRID", pal["grid"]), ("HOT", pal["gridHot"]),
             ("ACC", pal["accent"])]
    items += [("E%d%s" % (i + 1, "*" if i in pal["soloEnemies"] else ""), e) for i, e in enumerate(pal["enemyCandidates"])]
    img = cv.render(bloom=0.0)
    from PIL import ImageDraw
    d = ImageDraw.Draw(img)
    cell = w // len(items)
    for i, (label, e) in enumerate(items):
        rgb = tuple(int(e["hex"][k:k + 2], 16) for k in (1, 3, 5))
        d.rectangle([i * cell + 3, 3, (i + 1) * cell - 3, h - 22], fill=rgb)
        d.text((i * cell + 5, h - 18), "%s %s" % (label, e["hex"]), fill=(200, 200, 210))
    return img


def main():
    data = build()
    os.makedirs(os.path.join(DESIGN, "previews"), exist_ok=True)
    with open(os.path.join(DESIGN, "palettes.json"), "w", encoding="utf-8") as f:
        json.dump(data, f, indent=1)
    font = ld.load_font(os.path.join(DESIGN, "vector_font.txt"))
    from PIL import Image
    pw, ph, sh = 800, 450, 64
    sheet = Image.new("RGB", (pw * 2, (ph + sh) * ((len(data["palettes"]) + 1) // 2)), (0, 0, 0))
    for i, pal in enumerate(data["palettes"]):
        col, row = i % 2, i // 2
        sheet.paste(draw_panel(data, pal, font, pw, ph, seed=11 + i), (col * pw, row * (ph + sh)))
        sheet.paste(swatch_strip(data, pal, font, pw, sh), (col * pw, row * (ph + sh) + ph))
    out = os.path.join(DESIGN, "previews", "palettes.png")
    sheet.save(out, optimize=True)
    c = data["constants"]
    print("danger", c["danger"]["hex"], "| players", [p["hex"] for p in c["players"]], "| pickup", c["pickup"]["hex"])
    for pal in data["palettes"]:
        print("%-11s solo=%s four=%s  enemies=%s" % (pal["id"], pal["soloEnemies"], pal["fourPlayerEnemies"],
                                                      [e["hex"] for e in pal["enemyCandidates"]]))
    print("wrote", out)


if __name__ == "__main__":
    main()

"""RAVEFORM look variants: the target frame under each variant in design/look_variants.json.

The variants are the choice put to the player's eyes at the M1 checkpoint (design/look.md §8). This
sheet shows them on the same piece of the target frame so they can be compared before the engine can
draw them, and so the engine's versions can be checked against something.

Usage:  python design/tools/variants.py     -> design/previews/look_variants.png
"""
import json
import os
import sys

from PIL import Image, ImageDraw

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import frame_mock as fm  # noqa: E402

CROP = (500, 250, 1820, 830)     # the player, two kills and a stream of the swarm


def render(cv, v):
    hdr, rims, cores = cv.engine_hdr(width_scale=v["lineWidth"], pool_level=v["poolLevel"])
    return cv._composite(hdr, rims, cores, bloom=v["bloom"], weights=v["bloomWeights"], exposure=v["exposure"],
                         bg_level=v["bgLevel"], white_point=v["whitePoint"], bleed=v["bleed"])


def main():
    data = json.load(open(os.path.join(fm.DESIGN, "look_variants.json"), encoding="utf-8"))
    cv = fm.build("reef")
    w, h, strip = CROP[2] - CROP[0], CROP[3] - CROP[1], 30
    sheet = Image.new("RGB", (w, (h + strip) * len(data["variants"])), (0, 0, 0))
    draw = ImageDraw.Draw(sheet)
    for i, v in enumerate(data["variants"]):
        y = i * (h + strip)
        draw.text((10, y + 9), "%s  -  %s" % (v["name"].upper(), v["intent"]), fill=(215, 220, 235))
        sheet.paste(render(cv, v).crop(CROP), (0, y + strip))
        print("rendered", v["id"])
    out = os.path.join(fm.DESIGN, "previews", "look_variants.png")
    sheet.save(out, optimize=True)
    print("wrote", out)


if __name__ == "__main__":
    main()

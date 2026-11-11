"""Write an optimised review copy of a PNG, at most --max-width pixels wide (default 1600).

    python scripts/shrink_png.py captures/shot.png [out.png] [--max-width 1600]

Without an explicit output path the copy is written next to the source as <name>.small.png.
"""
import argparse
import sys

from PIL import Image


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("src")
    parser.add_argument("dst", nargs="?")
    parser.add_argument("--max-width", type=int, default=1600)
    args = parser.parse_args()

    dst = args.dst or (args.src[:-4] if args.src.lower().endswith(".png") else args.src) + ".small.png"
    with Image.open(args.src) as image:
        image = image.convert("RGB")
        if image.width > args.max_width:
            height = round(image.height * args.max_width / image.width)
            image = image.resize((args.max_width, height), Image.LANCZOS)
        image.save(dst, optimize=True)
        print(f"[png] {dst} {image.width}x{image.height}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

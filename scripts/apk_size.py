"""An APK's size by part (M7, release preparation): each native library, the dex code, the resources, the signature.

    python -I -B scripts/apk_size.py <apk>
"""
import os
import sys
import zipfile


def main() -> int:
    path = sys.argv[1]
    parts = {}
    with zipfile.ZipFile(path) as z:
        for i in z.infolist():
            n = i.filename
            if n.startswith("lib/"):
                k = n
            elif n.endswith(".dex"):
                k = "dex (Java: SDL's glue, the activity)"
            elif n.startswith("res/") or n == "resources.arsc":
                k = "resources (the icon and the rest)"
            elif n.startswith("META-INF/"):
                k = "signature"
            else:
                k = "other (" + n + ")" if n == "AndroidManifest.xml" else "other"
            parts[k] = parts.get(k, 0) + i.compress_size
    total = sum(parts.values())
    for k, v in sorted(parts.items(), key=lambda kv: -kv[1]):
        print("[size] %8.2f MB  %5.1f%%  %s" % (v / 1048576.0, 100.0 * v / total, k))
    print("[size] %8.2f MB  the APK (%.2f MB in its entries)" % (os.path.getsize(path) / 1048576.0, total / 1048576.0))
    return 0


if __name__ == "__main__":
    sys.exit(main())

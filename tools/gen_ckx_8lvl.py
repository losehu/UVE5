#!/usr/bin/env python3
"""Generate an 8-level grayscale lookup table from a JPEG.

Workflow requested:
- Load ckx.jpeg
- Resize to 448x320 (to account for LCD pixel aspect 3.5x5 mapping)
- Downsample to 128x64
- Quantize to 8 gray levels (0..7)
- Emit a C header with a row-major uint8_t array.

Usage:
  python3 tools/gen_ckx_8lvl.py \
    --in src/app/opencv/ckx.jpeg \
    --out src/app/assets/ckx_8lvl_128x64.h
"""

from __future__ import annotations

import argparse
from pathlib import Path

import cv2
import numpy as np


def quantize_to_levels(gray_u8: np.ndarray, levels: int) -> np.ndarray:
    if levels < 2:
        raise ValueError("levels must be >= 2")
    # Map 0..255 -> 0..levels-1 (rounded)
    max_level = levels - 1
    lvl = (gray_u8.astype(np.uint16) * max_level + 127) // 255
    return lvl.astype(np.uint8)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--in", dest="inp", required=True)
    ap.add_argument("--out", dest="out", required=True)
    ap.add_argument("--levels", type=int, default=8)
    ap.add_argument("--w", type=int, default=128)
    ap.add_argument("--h", type=int, default=64)
    ap.add_argument("--warp-w", type=int, default=448)
    ap.add_argument("--warp-h", type=int, default=320)
    args = ap.parse_args()

    in_path = Path(args.inp)
    out_path = Path(args.out)

    img = cv2.imread(str(in_path), cv2.IMREAD_COLOR)
    if img is None:
        raise SystemExit(f"Failed to read image: {in_path}")

    gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)

    # Step 1: warp to square-pixel representation of the LCD geometry.
    warped = cv2.resize(gray, (args.warp_w, args.warp_h), interpolation=cv2.INTER_AREA)

    # Step 2: downsample to LCD logical pixels.
    small = cv2.resize(warped, (args.w, args.h), interpolation=cv2.INTER_AREA)

    lvl = quantize_to_levels(small, args.levels)

    out_path.parent.mkdir(parents=True, exist_ok=True)

    name = "CKX_8LVL_128x64"
    with out_path.open("w", encoding="utf-8") as f:
        f.write("#pragma once\n")
        f.write("#include <stdint.h>\n\n")
        f.write("// Auto-generated from src/app/opencv/ckx.jpeg\n")
        f.write("// Pipeline: resize 448x320 -> downsample 128x64 -> quantize 8 levels (0..7)\n")
        f.write(f"static constexpr uint32_t {name}_W = {args.w};\n")
        f.write(f"static constexpr uint32_t {name}_H = {args.h};\n")
        f.write(f"static constexpr uint32_t {name}_LEVELS = {args.levels};\n")
        f.write("\n")
        f.write(f"static const uint8_t {name}[{args.w}*{args.h}] = {{\n")

        flat = lvl.reshape(-1)
        for y in range(args.h):
            row = flat[y * args.w : (y + 1) * args.w]
            f.write("  ")
            f.write(", ".join(str(int(v)) for v in row))
            f.write(",\n")

        f.write("};\n")

    print(f"Wrote {out_path} ({args.w}x{args.h}, {args.levels} levels)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

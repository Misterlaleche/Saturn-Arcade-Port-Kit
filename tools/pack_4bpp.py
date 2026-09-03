#!/usr/bin/env python3
"""Pack indexed pixels to Saturn VDP1 4bpp bytes.

Input is a raw file containing one 0..15 palette index per pixel. Width must be
an even number. Optionally remap one source transparent pen to Saturn pen 0 and
shift visible source pens into a supplied mapping.
"""

from __future__ import annotations

import argparse
from pathlib import Path


def remap_transparent(indices: bytes, transparent_pen: int) -> bytes:
    """Map transparent_pen -> 0 and remaining pens deterministically to 1..15."""
    transparent_pen &= 0x0F
    visible = [p for p in range(16) if p != transparent_pen]
    table = [0] * 16
    table[transparent_pen] = 0
    for dst, src in enumerate(visible, start=1):
        table[src] = dst
    return bytes(table[p & 0x0F] for p in indices)


def pack_nibbles(indices: bytes) -> bytes:
    if len(indices) & 1:
        raise ValueError("pixel count must be even")
    out = bytearray(len(indices) // 2)
    for i in range(0, len(indices), 2):
        out[i // 2] = ((indices[i] & 0x0F) << 4) | (indices[i + 1] & 0x0F)
    return bytes(out)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--transparent-pen", type=lambda x: int(x, 0), default=None)
    args = parser.parse_args()

    data = args.input.read_bytes()
    if args.transparent_pen is not None:
        data = remap_transparent(data, args.transparent_pen)
    args.output.write_bytes(pack_nibbles(data))


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Convert CPS1/CPS2-style 12-bit RGB + 4-bit brightness palette words.

This utility contains no game data. It is useful when an arcade source stores
palette entries in the common Capcom CPS format.
"""

from __future__ import annotations

import argparse


def cps_rgb888(word: int) -> tuple[int, int, int]:
    word &= 0xFFFF
    bright = 0x0F + (((word >> 12) & 0x0F) << 1)
    r = (((word >> 8) & 0x0F) * 0x11 * bright) // 0x2D
    g = (((word >> 4) & 0x0F) * 0x11 * bright) // 0x2D
    b = ((word & 0x0F) * 0x11 * bright) // 0x2D
    return min(r, 255), min(g, 255), min(b, 255)


def rgb888_to_saturn_rgb1555(r: int, g: int, b: int, msb: int = 1) -> int:
    return ((msb & 1) << 15) | ((b >> 3) << 10) | ((g >> 3) << 5) | (r >> 3)


def cps_saturn_rgb1555(word: int, msb: int = 1) -> int:
    return rgb888_to_saturn_rgb1555(*cps_rgb888(word), msb=msb)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("words", nargs="+", help="CPS palette words, e.g. F654 0xF543")
    args = parser.parse_args()

    for token in args.words:
        word = int(token, 16)
        rgb = cps_rgb888(word)
        sat = cps_saturn_rgb1555(word)
        print(f"{word:04X} -> RGB{rgb} -> Saturn 0x{sat:04X}")


if __name__ == "__main__":
    main()

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))

from cps_palette import cps_rgb888, cps_saturn_rgb1555
from pack_4bpp import pack_nibbles, remap_transparent


class PaletteTests(unittest.TestCase):
    def test_black(self):
        self.assertEqual(cps_rgb888(0x0000), (0, 0, 0))

    def test_saturn_range(self):
        value = cps_saturn_rgb1555(0xFFFF)
        self.assertTrue(0 <= value <= 0xFFFF)
        self.assertTrue(value & 0x8000)


class PackTests(unittest.TestCase):
    def test_pack(self):
        self.assertEqual(pack_nibbles(bytes([1, 2, 3, 4])), bytes([0x12, 0x34]))

    def test_transparent_remap(self):
        src = bytes([15, 0, 1, 14])
        dst = remap_transparent(src, 15)
        self.assertEqual(dst[0], 0)
        self.assertTrue(all(1 <= p <= 15 for p in dst[1:]))


if __name__ == "__main__":
    unittest.main()

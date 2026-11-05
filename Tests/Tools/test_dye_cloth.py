"""布地以外の画素を守る制作ツールの回帰テスト。"""
import importlib.util
import pathlib
import json
import subprocess
import sys
import tempfile
import unittest

from PIL import Image

ROOT = pathlib.Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("dye_cloth", ROOT / "tools/appearance/dye_cloth.py")
dye = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(dye)


class DyeTests(unittest.TestCase):
    def setUp(self):
        self.source = Image.new("RGBA", (4, 4), (40, 50, 80, 127))
        self.mask = Image.new("L", (4, 4))
        self.mask.putpixel((1, 1), 255)

    def test_outside_and_alpha_are_exact(self):
        result = dye.recolor(self.source, self.mask, "#C8CDD2", 55)
        self.assertEqual(result.getpixel((0, 0)), self.source.getpixel((0, 0)))
        self.assertNotEqual(result.getpixel((1, 1))[:3], self.source.getpixel((1, 1))[:3])
        self.assertEqual(result.getchannel("A").tobytes(), self.source.getchannel("A").tobytes())
        self.assertEqual(self.source.getpixel((1, 1)), (40, 50, 80, 127))
        self.assertEqual(dye.verify(self.source, result, self.mask)["changed_pixels"], 1)

    def test_texture_luminance_order_remains(self):
        self.mask.paste(255, (0, 0, 4, 4))
        self.source.putpixel((0, 0), (10, 15, 20, 255))
        result = dye.recolor(self.source, self.mask, "#C8CDD2", 55)
        self.assertLess(sum(result.getpixel((0, 0))[:3]), sum(result.getpixel((1, 1))[:3]))

    def test_outside_change_rejected(self):
        result = self.source.copy()
        result.putpixel((0, 0), (1, 2, 3, 127))
        with self.assertRaises(ValueError):
            dye.verify(self.source, result, self.mask)

    def test_alpha_change_rejected(self):
        result = self.source.copy()
        result.putpixel((1, 1), (40, 50, 80, 0))
        with self.assertRaises(ValueError):
            dye.verify(self.source, result, self.mask)

    def test_dimensions_rejected(self):
        with self.assertRaises(ValueError):
            dye.recolor(self.source, Image.new("L", (1, 1)), "#C8CDD2", 55)

    def test_non_binary_mask_rejected(self):
        self.mask.putpixel((1, 1), 50)
        with self.assertRaises(ValueError):
            dye.recolor(self.source, self.mask, "#C8CDD2", 55)

    def test_empty_mask_rejected(self):
        with self.assertRaises(ValueError):
            dye.recolor(self.source, Image.new("L", (4, 4)), "#C8CDD2", 55)

    def test_bad_color_and_reference_rejected(self):
        for color, reference in [("red", 55), ("#XXFFAA", 55), ("#C8CDD2", 0), ("#C8CDD2", 255)]:
            with self.subTest(color=color, reference=reference), self.assertRaises(ValueError):
                dye.recolor(self.source, self.mask, color, reference)

    def test_repeatable(self):
        first = dye.recolor(self.source, self.mask, "#C8CDD2", 55)
        second = dye.recolor(self.source, self.mask, "#C8CDD2", 55)
        self.assertEqual(first.tobytes(), second.tobytes())

    def test_mask_keeps_motif_and_protected_skin(self):
        self.source.putpixel((1, 1), (160, 145, 110, 255))
        layer = {"filter": "blue-cloth", "polygons": [[[0, 0], [3, 0], [3, 3], [0, 3]]]}
        mask = dye.make_mask(self.source, layer)
        self.assertEqual(mask.getpixel((1, 1)), 0)
        self.assertEqual(mask.getpixel((0, 0)), 255)

    def test_red_cord_excludes_blue_and_gold(self):
        self.source.putpixel((1, 1), (100, 30, 25, 255))
        self.source.putpixel((2, 2), (160, 145, 110, 255))
        layer = {"filter": "red-cord", "polygons": [[[0, 0], [3, 0], [3, 3], [0, 3]]]}
        mask = dye.make_mask(self.source, layer)
        self.assertEqual(mask.getpixel((1, 1)), 255)
        self.assertEqual(mask.getpixel((0, 0)), 0)
        self.assertEqual(mask.getpixel((2, 2)), 0)

    def test_unknown_extraction_rejected(self):
        with self.assertRaises(ValueError):
            dye.make_mask(self.source, {"filter": "unknown", "polygons": [[[0, 0], [3, 0], [3, 3]]]})

    def test_transparent_rgb_is_not_repainted(self):
        self.source.putpixel((1, 1), (40, 50, 80, 0))
        result = dye.recolor(self.source, self.mask, "#C8CDD2", 55)
        self.assertEqual(result.tobytes(), self.source.tobytes())

    def test_wrong_result_dimensions_rejected(self):
        with self.assertRaises(ValueError):
            dye.verify(self.source, Image.new("RGBA", (1, 1)), self.mask)

    def test_extreme_target_rejected(self):
        for color in ("#000000", "#FFFFFF"):
            with self.subTest(color=color), self.assertRaises(ValueError):
                dye.recolor(self.source, self.mask, color, 55)

    def test_cli_refuses_wrong_source_and_output_without_writes(self):
        template = json.loads((ROOT / "tools/appearance/hero-ren-dye1.json").read_text(encoding="utf-8-sig"))
        scratch = ROOT / "Saved/Tests/AppearancePython"
        scratch.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as directory:
            config_file = pathlib.Path(directory) / "config.json"
            for mutation in ("hash", "source", "destination"):
                config = dict(template)
                output = ROOT / "Saved/ArtPrototypes/Appearance/never-created-by-invalid-config"
                if mutation == "hash":
                    config["source_sha256"] = "0" * 64
                elif mutation == "source":
                    config["source"] = "README.md"
                else:
                    output = ROOT / "Content/Art/Characters"
                config_file.write_text(json.dumps(config), encoding="utf-8")
                completed = subprocess.run([sys.executable, str(ROOT / "tools/appearance/dye_cloth.py"), "--config", str(config_file), "--output", str(output)], capture_output=True, timeout=30)
                self.assertNotEqual(completed.returncode, 0)
                self.assertFalse((ROOT / "Saved/ArtPrototypes/Appearance/never-created-by-invalid-config").exists())


if __name__ == "__main__":
    unittest.main()

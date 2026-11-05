"""布地以外の画素を守る制作ツールの回帰テスト。"""
import importlib.util
import pathlib
import hashlib
import re
import json
import subprocess
import sys
import tempfile
import unittest
import uuid

from PIL import Image, ImageChops

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

    def test_other_cloth_colors_keep_gold_outside_mask(self):
        for kind, rgb in [("cool-cloth", (60, 56, 67)), ("green-cloth", (45, 62, 48)),
                          ("pale-cloth", (205, 198, 182)), ("warm-cloth", (90, 65, 48)),
                          ("neutral-cloth", (65, 59, 55))]:
            with self.subTest(kind=kind):
                image = Image.new("RGBA", (4, 4), rgb + (255,))
                image.putpixel((1, 1), (160, 145, 110, 255))
                layer = {"filter": kind, "polygons": [[[0, 0], [3, 0], [3, 3], [0, 3]]]}
                mask = dye.make_mask(image, layer)
                self.assertEqual(mask.getpixel((0, 0)), 255)
                self.assertEqual(mask.getpixel((1, 1)), 0)

    def test_local_exclusion_can_be_colored_by_other_layer(self):
        polygon = [[1, 1], [2, 1], [2, 2], [1, 2]]
        primary = {"filter": "blue-cloth", "polygons": [[[0, 0], [3, 0], [3, 3], [0, 3]]], "excluded_polygons": [polygon]}
        secondary = {"filter": "blue-cloth", "polygons": [polygon]}
        mask = dye.make_mask(self.source, primary)
        self.assertEqual(mask.getpixel((1, 1)), 0)
        result = dye.recolor(self.source, mask, "#C8CDD2", 55)
        result = dye.recolor(result, dye.make_mask(self.source, secondary), "#243B5A", 55)
        self.assertNotEqual(result.getpixel((1, 1)), self.source.getpixel((1, 1)))
        self.assertEqual(dye.verify_protected(self.source, result, [primary, secondary]), 0)

    def test_protected_polygon_overrides_blue_cloth(self):
        layer = {"filter": "blue-cloth", "polygons": [[[0, 0], [3, 0], [3, 3], [0, 3]]],
                 "protected_polygons": [[[1, 1], [2, 1], [2, 2], [1, 2]]]}
        mask = dye.make_mask(self.source, layer)
        result = dye.recolor(self.source, mask, "#C8CDD2", 55)
        self.assertEqual(mask.getpixel((1, 1)), 0)
        self.assertEqual(result.getpixel((1, 1)), self.source.getpixel((1, 1)))
        self.assertNotEqual(result.getpixel((0, 0)), self.source.getpixel((0, 0)))

    def test_fully_protected_mask_is_rejected(self):
        polygon = [[0, 0], [3, 0], [3, 3], [0, 3]]
        with self.assertRaises(ValueError):
            dye.make_mask(self.source, {"filter": "blue-cloth", "polygons": [polygon], "protected_polygons": [polygon]})

    def test_final_protection_checks_all_layers(self):
        layers = [{"protected_polygons": [[[1, 1], [2, 1], [2, 2], [1, 2]]]}, {}]
        result = self.source.copy()
        result.putpixel((1, 1), (100, 110, 120, 127))
        with self.assertRaises(ValueError):
            dye.verify_protected(self.source, result, layers)

    def test_unchanged_protection_is_reported(self):
        layers = [{"protected_polygons": [[[1, 1], [2, 1], [2, 2], [1, 2]]]}]
        self.assertEqual(dye.verify_protected(self.source, self.source.copy(), layers), 4)

    def test_valid_layer_names(self):
        self.assertIsNone(dye.validate_layer_names([{"name": "primary"}, {"name": "secondary_2"}]))

    def test_bad_and_duplicate_layer_names_rejected(self):
        for layers in ([], None, {}, [None], [{}], [{"name": "../escape"}], [{"name": "x/y"}],
                       [{"name": "x\\y"}], [{"name": ""}], [{"name": 2}], [{"name": "x" * 65}],
                       [{"name": "primary"}, {"name": "PRIMARY"}]):
            with self.subTest(layers=layers), self.assertRaises(ValueError):
                dye.validate_layer_names(layers)

    def test_cli_bad_layer_name_never_creates_output(self):
        template = json.loads((ROOT / "tools/appearance/hero-ren-dye1.json").read_text(encoding="utf-8-sig"))
        template["layers"][0]["name"] = "../escape"
        output = ROOT / "Saved/ArtPrototypes/Appearance" / ("invalid-layer-proof-" + uuid.uuid4().hex)
        scratch = ROOT / "Saved/Tests/AppearancePython"
        scratch.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=scratch) as directory:
            config_file = pathlib.Path(directory) / "config.json"
            config_file.write_text(json.dumps(template), encoding="utf-8")
            completed = subprocess.run([sys.executable, str(ROOT / "tools/appearance/dye_cloth.py"), "--config", str(config_file), "--output", str(output)], capture_output=True, timeout=30)
            self.assertNotEqual(completed.returncode, 0)
            self.assertFalse(output.exists(), "不正設定では候補画像も作成しない")

    def test_nine_recipes_match_approved_palette_and_originals(self):
        table = (ROOT / "specs/001-girls-spire/appearance-palette-proposal.md").read_text(encoding="utf-8-sig")
        count = 0
        for row in table.splitlines():
            name = re.search(r"Characters/([a-z-]+)-v1\.png", row)
            if not name:
                continue
            colors = re.findall(r"#[0-9A-Fa-f]{6}", row)
            recipe = json.loads((ROOT / f"tools/appearance/{name[1]}-dye1.json").read_text(encoding="utf-8-sig"))
            self.assertEqual([item["color"].upper() for item in recipe["layers"]], colors)
            source = ROOT / recipe["source"]
            self.assertEqual(hashlib.sha256(source.read_bytes()).hexdigest(), recipe["source_sha256"])
            with Image.open(source) as original:
                self.assertEqual(list(original.size), recipe["size"])
            self.assertTrue(all(item["protected_polygons"] for item in recipe["layers"]))
            dye.validate_layer_names(recipe["layers"])
            count += 1
        self.assertEqual(count, 9)

    def test_reviewed_variants_match_manifest(self):
        reviewed = json.loads((ROOT / "tools/appearance/reviewed-portraits.json").read_text(encoding="utf-8"))
        self.assertEqual(sorted(item["actor_id"] for item in reviewed), list(range(9)))
        for item in reviewed:
            with self.subTest(actor=item["actor_id"]):
                path = ROOT / item["alternate"]
                self.assertEqual(hashlib.sha256(path.read_bytes()).hexdigest(), item["alternate_sha256"])
                with Image.open(path) as image:
                    self.assertEqual(image.size, (1024, 1536))

    def test_all_reviewed_images_keep_noncloth_and_alpha(self):
        reviewed = json.loads((ROOT / "tools/appearance/reviewed-portraits.json").read_text(encoding="utf-8"))
        for item in reviewed:
            with self.subTest(actor=item["actor_id"]):
                recipe = json.loads((ROOT / item["config"]).read_text(encoding="utf-8-sig"))
                with Image.open(ROOT / item["standard"]) as image:
                    source = image.convert("RGBA")
                with Image.open(ROOT / item["alternate"]) as image:
                    result = image.convert("RGBA")
                mask = Image.new("L", source.size)
                for layer in recipe["layers"]:
                    mask = ImageChops.lighter(mask, dye.make_mask(source, layer))
                self.assertGreater(dye.verify(source, result, mask)["changed_pixels"], 0)
                self.assertEqual(dye.verify_protected(source, result, recipe["layers"]), item["protected_pixels"])

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

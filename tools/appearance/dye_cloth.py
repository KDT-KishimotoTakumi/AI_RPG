"""承認済み布地マスクだけを色替えする、ゲーム非依存の制作ツール。"""
import argparse
import hashlib
import json
import pathlib
import re

from PIL import Image, ImageDraw


def check_mask(source, mask):
    if source.mode != "RGBA" or mask.mode != "L" or source.size != mask.size:
        raise ValueError("RGBA原画と同寸法のLマスクが必要")
    values = set(mask.getdata())
    if not values <= {0, 255} or 255 not in values:
        raise ValueError("空でない二値マスクが必要")


def recolor(source, mask, color, reference):
    check_mask(source, mask)
    if not re.fullmatch(r"#[0-9A-Fa-f]{6}", color) or not 0 < reference < 255:
        raise ValueError("HEX基準色と0より大きく255未満の原画基準輝度が必要")
    target = tuple(int(color[index:index + 2], 16) for index in (1, 3, 5))
    target_luma = sum(value * weight for value, weight in zip(target, (0.2126, 0.7152, 0.0722)))
    if target in ((0, 0, 0), (255, 255, 255)):
        raise ValueError("陰影を保つため白黒の極値は指定不可")
    gain = (target_luma / (255 - target_luma)) / (reference / (255 - reference))
    output = source.copy()
    pixels = []
    for pixel, selected in zip(source.getdata(), mask.getdata()):
        if not selected or not pixel[3]:
            pixels.append(pixel)
            continue
        luma = sum(value * weight for value, weight in zip(pixel[:3], (0.2126, 0.7152, 0.0722))) / 255
        # 単調な曲線で濃淡を残し、単純な乗算による白飛びを避ける。
        shifted = 255 * luma * gain / (1 - luma + luma * gain)
        pixels.append(tuple(min(255, max(0, round(value * shifted / target_luma))) for value in target) + (pixel[3],))
    output.putdata(pixels)
    return output


def verify(source, result, mask):
    check_mask(source, mask)
    if source.mode != result.mode or source.size != result.size:
        raise ValueError("画像寸法/形式が変化")
    changed = 0
    for before, after, selected in zip(source.getdata(), result.getdata(), mask.getdata()):
        if before[3] != after[3] or (not selected and before != after):
            raise ValueError("非対象RGBAまたは透過が変化")
        changed += before != after
    return {"changed_pixels": changed, "outside_rgba_identical": True, "alpha_identical": True}


def make_mask(source, layer):
    mask = Image.new("L", source.size)
    draw = ImageDraw.Draw(mask)
    for polygon in layer["polygons"]:
        draw.polygon([tuple(point) for point in polygon], fill=255)
    selected = []
    for pixel, inside in zip(source.getdata(), mask.getdata()):
        red, green, blue, alpha = pixel
        if layer["filter"] == "blue-cloth":
            keep = blue > red * 1.12 and blue > green * 1.06
        elif layer["filter"] == "red-cord":
            keep = red > green * 1.35 and red > blue * 1.35
        else:
            raise ValueError("未定義の布地抽出条件")
        selected.append(255 if inside and keep and alpha else 0)
    mask.putdata(selected)
    check_mask(source, mask)
    return mask


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", type=pathlib.Path, required=True)
    parser.add_argument("--output", type=pathlib.Path, required=True)
    args = parser.parse_args()
    root = pathlib.Path(__file__).resolve().parents[2]
    config = json.loads(args.config.read_text(encoding="utf-8-sig"))
    original = (root / config["source"]).resolve()
    output = args.output.resolve()
    if not original.is_relative_to(root / "Content/Art/Characters") or not output.is_relative_to(root / "Saved/ArtPrototypes"):
        raise ValueError("原画は人物素材、出力は未採用候補専用領域に限定")
    source_hash = hashlib.sha256(original.read_bytes()).hexdigest()
    if source_hash != config["source_sha256"].lower():
        raise ValueError("原画SHAが定義時と異なるため停止")
    with Image.open(original) as image:
        source = image.convert("RGBA")
    if list(source.size) != config["size"]:
        raise ValueError("原画寸法が定義時と異なるため停止")
    result = source.copy()
    union = Image.new("L", source.size)
    masks = []
    for layer in config["layers"]:
        mask = make_mask(source, layer)
        if any(a and b for a, b in zip(union.getdata(), mask.getdata())):
            raise ValueError("主色/副色マスクが重複")
        union.putdata([max(a, b) for a, b in zip(union.getdata(), mask.getdata())])
        result = recolor(result, mask, layer["color"], layer["reference_luma"])
        masks.append((layer["name"], mask))
    report = verify(source, result, union)
    if hashlib.sha256(original.read_bytes()).hexdigest() != source_hash:
        raise ValueError("処理中に原画が変化")
    output.mkdir(parents=True, exist_ok=False)
    result.save(output / "candidate.png", optimize=True)
    union.save(output / "mask.png", optimize=True)
    overlay = source.copy()
    tint = Image.new("RGBA", source.size, (255, 0, 160, 255))
    overlay.paste(Image.blend(source, tint, 0.45), (0, 0), union)
    overlay.save(output / "mask-overlay.png", optimize=True)
    for name, mask in masks:
        mask.save(output / f"mask-{name}.png", optimize=True)
    report.update(source_sha256=source_hash, size=list(source.size), config=config, status="目視確認待ち・ゲーム未接続")
    (output / "report.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps({key: report[key] for key in ("changed_pixels", "outside_rgba_identical", "alpha_identical", "status")}, ensure_ascii=False))


if __name__ == "__main__":
    main()

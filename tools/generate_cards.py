#!/usr/bin/env python3
"""Generate Xteink X4 card BMP images from a text list."""

from __future__ import annotations

import argparse
import os
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

# Portrait layout used by the firmware (display rotation 3).
WIDTH = 480
HEIGHT = 800
MARGIN = 36
FRAME = 6
COVER_TITLE = "CARD SHUFFLER"


def load_font(font_path: str | None, size: int) -> ImageFont.FreeTypeFont | ImageFont.ImageFont:
    if font_path and Path(font_path).is_file():
        return ImageFont.truetype(font_path, size)
    try:
        return ImageFont.truetype("DejaVuSans.ttf", size)
    except OSError:
        return ImageFont.load_default()


def wrap_text(text: str, font: ImageFont.ImageFont, max_width: int) -> list[str]:
    words = text.replace("\n", " ").split()
    if not words:
        return [""]

    lines: list[str] = []
    current = words[0]
    for word in words[1:]:
        probe = f"{current} {word}"
        if font.getlength(probe) <= max_width:
            current = probe
        else:
            lines.append(current)
            current = word
    lines.append(current)
    return lines


def draw_card(text: str, font: ImageFont.ImageFont, title: str | None = None) -> Image.Image:
    image = Image.new("1", (WIDTH, HEIGHT), 1)
    draw = ImageDraw.Draw(image)

    inner_left = MARGIN
    inner_top = MARGIN
    inner_right = WIDTH - MARGIN
    inner_bottom = HEIGHT - MARGIN

    for offset in range(FRAME):
        draw.rectangle(
            [
                inner_left + offset,
                inner_top + offset,
                inner_right - offset,
                inner_bottom - offset,
            ],
            outline=0,
        )

    content_left = inner_left + FRAME + 20
    content_right = inner_right - FRAME - 20
    content_width = content_right - content_left

    y = inner_top + FRAME + 28
    if title:
        title_font = load_font(None, max(28, int(font.size * 0.9)))
        draw.text((WIDTH // 2, y), title, font=title_font, fill=0, anchor="ma")
        y += int(title_font.size * 1.8)

    lines = wrap_text(text, font, content_width)
    if len(lines) > 8:
        lines = lines[:7] + ["…"]

    line_height = int(font.size * 1.45)
    block_height = len(lines) * line_height
    start_y = (HEIGHT - block_height) // 2

    for i, line in enumerate(lines):
        draw.text((WIDTH // 2, start_y + i * line_height), line, font=font, fill=0, anchor="mm")

    return image


def read_lines(path: Path) -> list[str]:
    lines: list[str] = []
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if line and not line.startswith("#"):
            lines.append(line)
    return lines


def save_bmp(image: Image.Image, path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    image.save(path, format="BMP")


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate Xteink X4 BMP card deck")
    parser.add_argument("text_file", help="UTF-8 text file, one card per line")
    parser.add_argument("output_dir", help="Output folder (e.g. ./aaa)")
    parser.add_argument("--font", dest="font_path", help="TTF/OTF font path")
    parser.add_argument("--font-size", type=int, default=42, help="Font size (default: 42)")
    parser.add_argument("--cover-title", default=COVER_TITLE, help="Cover title text")
    args = parser.parse_args()

    text_path = Path(args.text_file)
    output_dir = Path(args.output_dir)

    if not text_path.is_file():
        raise SystemExit(f"Text file not found: {text_path}")

    cards = read_lines(text_path)
    if not cards:
        raise SystemExit("No card lines found in text file")

    font = load_font(args.font_path, args.font_size)

    cover = draw_card(args.cover_title, font, title=None)
    save_bmp(cover, output_dir / "cover.bmp")

    for index, text in enumerate(cards):
        card = draw_card(text, font)
        filename = f"{index:03d}.bmp"
        save_bmp(card, output_dir / filename)

    print(f"Generated cover.bmp and {len(cards)} cards in {output_dir.resolve()}")
    print(f"Image size: {WIDTH}x{HEIGHT}, 1-bit BMP")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

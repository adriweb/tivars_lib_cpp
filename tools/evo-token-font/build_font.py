#!/usr/bin/env python3
"""Build a one-codepoint-per-Evo-token readable OpenType font."""

from __future__ import annotations

import argparse
import json
import unicodedata
from pathlib import Path

from fontTools.fontBuilder import FontBuilder
from fontTools.pens.recordingPen import DecomposingRecordingPen
from fontTools.pens.transformPen import TransformPen
from fontTools.pens.ttGlyphPen import TTGlyphPen
from fontTools.ttLib import TTFont


FONT_FAMILY = "Evo Token Readable"
FONT_VERSION = "1.000"
UPM = 1000
ASCENT = 900
DESCENT = -300
SIDE_BEARING = 70
LETTER_SPACING = 22

# Noto deliberately leaves these compatibility/private characters unmapped.
# Use a readable approximation for the compatibility forms and an explicit
# codepoint label for the two private characters instead of a tofu box.
CHAR_REPLACEMENTS = {
    "\uFE62": "+",
    "\uFF26": "F",
    "\uFF3C": "\\",
    "\uF04D": "[U+F04D]",
    "\U000F83F5": "[U+F83F5]",
}


def arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--mapping", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--source-font", type=Path, action="append", required=True)
    return parser.parse_args()


def font_sources(paths: list[Path]) -> list[dict]:
    sources = []
    for path in paths:
        font = TTFont(path)
        sources.append({
            "path": path,
            "font": font,
            "cmap": font.getBestCmap(),
            "glyphs": font.getGlyphSet(),
            "metrics": font["hmtx"].metrics,
            "scale": UPM / font["head"].unitsPerEm,
        })
    return sources


def source_for(codepoint: int, sources: list[dict]) -> tuple[dict, str] | None:
    for source in sources:
        glyph_name = source["cmap"].get(codepoint)
        if glyph_name is not None:
            return source, glyph_name
    return None


def replace_unsupported(text: str, sources: list[dict]) -> tuple[str, list[str]]:
    rendered = []
    fallbacks = []
    for character in text:
        if source_for(ord(character), sources) is not None:
            rendered.append(character)
            continue
        replacement = CHAR_REPLACEMENTS.get(character, f"[U+{ord(character):04X}]")
        rendered.append(replacement)
        fallbacks.append(f"U+{ord(character):04X} -> {replacement}")
    return "".join(rendered), fallbacks


def draw_text(text: str, sources: list[dict]):
    pen = TTGlyphPen(None)
    cursor = SIDE_BEARING
    previous_advance = 0

    for character in text:
        found = source_for(ord(character), sources)
        if found is None:
            raise RuntimeError(f"No source glyph for U+{ord(character):04X}")
        source, source_name = found
        scale = source["scale"]
        advance, _ = source["metrics"][source_name]
        advance = int(round(advance * scale))

        # Zero-width combining marks in Noto carry their own negative bearing.
        # Drawing at the current cursor lets them overlay the previous glyph.
        transform = (scale, 0, 0, scale, cursor, 0)
        recording = DecomposingRecordingPen(source["glyphs"])
        source["glyphs"][source_name].draw(recording)
        recording.replay(TransformPen(pen, transform))

        if not unicodedata.combining(character):
            cursor += advance + LETTER_SPACING
            previous_advance = advance
        elif previous_advance == 0:
            cursor += max(advance, UPM // 2)

    width = max(UPM // 2, cursor - LETTER_SPACING + SIDE_BEARING)
    return pen.glyph(), width


def notdef_glyph():
    pen = TTGlyphPen(None)
    pen.moveTo((80, -120))
    pen.lineTo((520, -120))
    pen.lineTo((520, 760))
    pen.lineTo((80, 760))
    pen.closePath()
    pen.moveTo((145, -40))
    pen.lineTo((455, 680))
    pen.lineTo((390, 680))
    pen.lineTo((80, -40))
    pen.closePath()
    pen.moveTo((455, -40))
    pen.lineTo((145, 680))
    pen.lineTo((80, 680))
    pen.lineTo((390, -40))
    pen.closePath()
    return pen.glyph()


def build(mapping: dict, sources: list[dict], output_dir: Path) -> None:
    tokens = mapping["tokens"]
    glyph_order = [".notdef"]
    glyphs = {".notdef": notdef_glyph()}
    metrics = {".notdef": (600, 40)}
    cmap = {}
    fallback_count = 0

    for token in tokens:
        glyph_name = f"evo{token['hex']}"
        font_text, fallbacks = replace_unsupported(token["glyphText"], sources)
        token["fontText"] = font_text
        token["fontFallbacks"] = fallbacks
        fallback_count += bool(fallbacks)
        glyph, width = draw_text(font_text, sources)
        glyph_order.append(glyph_name)
        glyphs[glyph_name] = glyph
        metrics[glyph_name] = (width, SIDE_BEARING)
        cmap[token["value"]] = glyph_name

    builder = FontBuilder(UPM, isTTF=True)
    builder.setupGlyphOrder(glyph_order)
    builder.setupCharacterMap(cmap)
    builder.setupGlyf(glyphs, calcGlyphBounds=True)
    builder.setupHorizontalMetrics(metrics)
    builder.setupHorizontalHeader(ascent=ASCENT, descent=DESCENT)
    builder.setupNameTable({
        "familyName": FONT_FAMILY,
        "styleName": "Regular",
        "uniqueFontIdentifier": f"EvoTokenReadable-Regular-{FONT_VERSION}",
        "fullName": f"{FONT_FAMILY} Regular",
        "psName": "EvoTokenReadable-Regular",
        "version": f"Version {FONT_VERSION}",
        "manufacturer": "tivars_lib_cpp contributors",
        "designer": "Generated from tivars_lib_cpp EvoFormat mappings",
        "description": "Visualizes each recognized 16-bit Evo token as one readable glyph.",
        "licenseDescription": "SIL Open Font License, Version 1.1",
        "licenseInfoURL": "https://openfontlicense.org",
    })
    builder.setupOS2(
        sTypoAscender=ASCENT,
        sTypoDescender=DESCENT,
        usWinAscent=ASCENT,
        usWinDescent=-DESCENT,
        sxHeight=536,
        sCapHeight=714,
    )
    builder.setupPost(keepGlyphNames=True)
    builder.setupMaxp()

    output_dir.mkdir(parents=True, exist_ok=True)
    ttf_path = output_dir / "EvoTokenReadable-Regular.ttf"
    woff2_path = output_dir / "EvoTokenReadable-Regular.woff2"
    builder.save(ttf_path)

    web_font = TTFont(ttf_path)
    web_font.flavor = "woff2"
    web_font.save(woff2_path)

    enriched_mapping = dict(mapping)
    enriched_mapping["fontFamily"] = FONT_FAMILY
    enriched_mapping["fallbackTokenCount"] = fallback_count
    enriched_mapping["sourceFonts"] = [source["path"].name for source in sources]
    (output_dir / "evo-token-map.json").write_text(
        json.dumps(enriched_mapping, ensure_ascii=False, indent=2) + "\n",
        encoding="utf-8",
    )
    (output_dir / "evo-token-map.js").write_text(
        "window.EVO_TOKEN_MAP = "
        + json.dumps(enriched_mapping, ensure_ascii=False, separators=(",", ":"))
        + ";\n",
        encoding="utf-8",
    )

    print(f"Built {len(tokens)} token glyphs ({fallback_count} with explicit visual fallbacks)")
    print(ttf_path)
    print(woff2_path)


def main() -> None:
    args = arguments()
    mapping = json.loads(args.mapping.read_text(encoding="utf-8"))
    sources = font_sources(args.source_font)
    build(mapping, sources, args.output_dir)


if __name__ == "__main__":
    main()
